// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "detail/signaling_channel.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <algorithm>

#include "decart/errors.h"
#include "detail/url.h"

namespace decart::detail {
namespace {

struct NetSystemInit {
  NetSystemInit() { ix::initNetSystem(); }
};
void ensureNetSystem() { static NetSystemInit once; }

} // namespace

SignalingChannel::SignalingChannel(std::string url, std::string userAgent)
    : url_(std::move(url)), userAgent_(std::move(userAgent)) {
  ensureNetSystem();
}

SignalingChannel::~SignalingChannel() { close(); }

void SignalingChannel::setCallbacks(Callbacks callbacks) { callbacks_ = std::move(callbacks); }

bool SignalingChannel::isOpen() const { return open_; }

RoomInfo SignalingChannel::openAndJoin(std::chrono::milliseconds connectTimeout) {
  // Single deadline shared across socket-open and room-join phases.
  const auto deadline = std::chrono::steady_clock::now() + connectTimeout;
  const std::string finalUrl = appendQuery(url_, "user_agent", userAgent_);

  ws_ = std::make_unique<ix::WebSocket>();
  ws_->setUrl(finalUrl);
  ws_->disableAutomaticReconnection();
  ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
    switch (msg->type) {
      case ix::WebSocketMessageType::Open: {
        std::lock_guard<std::mutex> lk(mutex_);
        open_ = true;
        cv_.notify_all();
        break;
      }
      case ix::WebSocketMessageType::Message:
        onMessage(msg->str);
        break;
      case ix::WebSocketMessageType::Close:
        onClosed(msg->closeInfo.code, msg->closeInfo.reason);
        break;
      case ix::WebSocketMessageType::Error:
        onSocketError(msg->errorInfo.reason);
        break;
      default:
        break;
    }
  });
  ws_->start();

  // 1. Wait for the socket to open (against the shared deadline).
  {
    std::unique_lock<std::mutex> lk(mutex_);
    if (!cv_.wait_until(lk, deadline, [&] { return open_ || fatalError_.has_value(); })) {
      throw Exception(Error{ErrorCode::WebsocketError, "WebSocket open timeout"});
    }
    if (fatalError_) throw Exception(Error{ErrorCode::WebsocketError, *fatalError_});
  }

  // 2. Arm a waiter for room_info, then send the join request.
  auto waiter = std::make_shared<Waiter>();
  waiter->match = [](const IncomingMessage& m) { return m.type == IncomingType::LiveKitRoomInfo; };
  {
    std::lock_guard<std::mutex> lk(mutex_);
    waiters_.push_back(waiter);
    // Continue against the same overall deadline; queue_position extends it by a
    // fresh connectTimeout so queue waiting does not count against the bound.
    handshakeWindow_ = connectTimeout;
    handshakeDeadline_ = deadline;
  }
  if (!sendText(makeJoinMsg(std::nullopt).dump())) {
    std::lock_guard<std::mutex> lk(mutex_);
    waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());
    throw Exception(Error{ErrorCode::WebsocketError, "Failed to send join"});
  }

  // 3. Wait for room_info; queue_position pushes the deadline out.
  {
    std::unique_lock<std::mutex> lk(mutex_);
    while (!waiter->ready && !fatalError_) {
      if (cv_.wait_until(lk, handshakeDeadline_) == std::cv_status::timeout &&
          std::chrono::steady_clock::now() >= handshakeDeadline_) {
        break;
      }
    }
    waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());
    if (fatalError_) throw Exception(Error{ErrorCode::ServerError, *fatalError_});
    if (!waiter->ready) throw Exception(Error{ErrorCode::TimeoutError, "livekit_room_info timeout"});
    connected_ = true;
    return RoomInfo{waiter->result.livekitUrl, waiter->result.token, waiter->result.roomName,
                    waiter->result.sessionId};
  }
}

IncomingMessage SignalingChannel::request(const nlohmann::json& message,
                                          std::function<bool(const IncomingMessage&)> match,
                                          std::chrono::milliseconds timeout, const char* label) {
  // One ack-bearing request in flight at a time (see requestMutex_).
  std::lock_guard<std::mutex> serialize(requestMutex_);

  auto waiter = std::make_shared<Waiter>();
  waiter->match = std::move(match);
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (fatalError_) throw Exception(Error{ErrorCode::SignalingError, *fatalError_});
    if (!open_) throw Exception(Error{ErrorCode::NotConnected, "Signaling channel not open"});
    waiters_.push_back(waiter);
  }

  if (!sendText(message.dump())) {
    std::lock_guard<std::mutex> lk(mutex_);
    waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());
    throw Exception(Error{ErrorCode::WebsocketError, "WebSocket is not open"});
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::unique_lock<std::mutex> lk(mutex_);
  cv_.wait_until(lk, deadline, [&] { return waiter->ready || fatalError_.has_value(); });
  waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());

  if (waiter->ready) {
    if (!waiter->result.success) {
      throw Exception(
          Error{ErrorCode::ServerError, waiter->result.error.value_or(std::string(label) + " failed")});
    }
    return waiter->result;
  }
  if (fatalError_) throw Exception(Error{ErrorCode::SignalingError, *fatalError_});
  throw Exception(Error{ErrorCode::TimeoutError, std::string(label) + " timed out"});
}

void SignalingChannel::sendPrompt(const std::string& text, bool enhance, std::chrono::milliseconds timeout) {
  request(
      makePromptMsg(text, enhance),
      [text](const IncomingMessage& m) { return m.type == IncomingType::PromptAck && m.prompt == text; },
      timeout, "Prompt send");
}

void SignalingChannel::setImageData(const std::optional<std::string>& base64,
                                    const std::optional<std::string>& prompt,
                                    const std::optional<bool>& enhance, std::chrono::milliseconds timeout) {
  request(
      makeSetImageDataMsg(base64, prompt, enhance),
      [](const IncomingMessage& m) { return m.type == IncomingType::SetImageAck; }, timeout, "Image send");
}

void SignalingChannel::setImageRef(const std::string& ref, const std::optional<std::string>& prompt,
                                   const std::optional<bool>& enhance, std::chrono::milliseconds timeout) {
  request(
      makeSetImageRefMsg(ref, prompt, enhance),
      [](const IncomingMessage& m) { return m.type == IncomingType::SetImageAck; }, timeout, "Image send");
}

void SignalingChannel::onMessage(const std::string& text) {
  const IncomingMessage m = parseIncoming(text);
  if (m.type == IncomingType::Unknown) return;

  // Acks and room_info are consumed by their waiters; everything else is an event.
  {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& w : waiters_) {
      if (!w->ready && w->match(m)) {
        w->result = m;
        w->ready = true;
        cv_.notify_all();
        return;
      }
    }
  }

  switch (m.type) {
    case IncomingType::QueuePosition: {
      {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!connected_) {
          handshakeDeadline_ = std::chrono::steady_clock::now() + handshakeWindow_;
          cv_.notify_all();
        }
      }
      if (callbacks_.onQueuePosition) callbacks_.onQueuePosition(m.position, m.queueSize);
      break;
    }
    case IncomingType::GenerationStarted:
      if (callbacks_.onGenerationStarted) callbacks_.onGenerationStarted();
      break;
    case IncomingType::GenerationTick:
      if (callbacks_.onGenerationTick) callbacks_.onGenerationTick(m.seconds);
      break;
    case IncomingType::GenerationEnded:
      if (callbacks_.onGenerationEnded) callbacks_.onGenerationEnded(m.seconds, m.reason);
      break;
    case IncomingType::ServerError: {
      {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!fatalError_) fatalError_ = m.errorMessage;
        cv_.notify_all();
      }
      if (callbacks_.onServerError) callbacks_.onServerError(m.errorMessage);
      break;
    }
    default:
      break;
  }
}

void SignalingChannel::onClosed(int code, const std::string& reason) {
  bool report = false;
  {
    std::lock_guard<std::mutex> lk(mutex_);
    open_ = false;
    const bool wasConnected = connected_;
    if (!fatalError_) {
      fatalError_ = "WebSocket closed: code=" + std::to_string(code) + (reason.empty() ? "" : " " + reason);
    }
    cv_.notify_all();
    report = wasConnected && !closing_;
    closing_ = true; // suppress duplicate reports from a trailing Error/Close
  }
  if (report && callbacks_.onClosed) callbacks_.onClosed(code, reason);
}

void SignalingChannel::onSocketError(const std::string& reason) {
  std::lock_guard<std::mutex> lk(mutex_);
  if (!fatalError_) fatalError_ = reason.empty() ? "WebSocket error" : reason;
  cv_.notify_all();
}

bool SignalingChannel::sendText(const std::string& text) {
  if (!ws_) return false;
  return ws_->sendText(text).success;
}

void SignalingChannel::close() {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (closing_ && !ws_) return;
    closing_ = true;
  }
  // stop() joins the WS thread; must not hold the lock (the Close callback locks).
  if (ws_) ws_->stop();
  {
    std::lock_guard<std::mutex> lk(mutex_);
    open_ = false;
    if (!fatalError_) fatalError_ = "Signaling channel closed";
    cv_.notify_all();
  }
}

} // namespace decart::detail
