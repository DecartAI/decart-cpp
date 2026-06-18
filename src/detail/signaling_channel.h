// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "realtime/messages.h"

namespace ix {
class WebSocket;
}

namespace decart::detail {

/// LiveKit room credentials returned by the signaling handshake.
struct RoomInfo {
  std::string livekitUrl;
  std::string token;
  std::string roomName;
  std::string sessionId;
};

/// The Decart realtime signaling channel: a WebSocket to `wss://api3.decart.ai`
/// that performs the join handshake, carries prompt/image control messages
/// (request/ack), and forwards server events. Media flows separately over
/// LiveKit. See [[decart-realtime-wire-protocol]].
///
/// Thread-safety: `sendPrompt`/`setImage*` may be called from any thread.
/// Server events are delivered on the WebSocket's internal thread.
class SignalingChannel {
public:
  /// Server-pushed event callbacks. All optional; invoked on the WS thread.
  struct Callbacks {
    std::function<void(int position, int queueSize)> onQueuePosition;
    std::function<void()> onGenerationStarted;
    std::function<void(double seconds)> onGenerationTick;
    std::function<void(double seconds, const std::string& reason)> onGenerationEnded;
    std::function<void(const std::string& message)> onServerError;
    /// Unexpected close after a successful connect (code, reason).
    std::function<void(int code, const std::string& reason)> onClosed;
  };

  /// @param url        Full signaling URL including query params.
  /// @param userAgent  Value appended as the `user_agent` query param.
  SignalingChannel(std::string url, std::string userAgent);
  ~SignalingChannel();

  SignalingChannel(const SignalingChannel&) = delete;
  SignalingChannel& operator=(const SignalingChannel&) = delete;

  void setCallbacks(Callbacks callbacks);

  /// Open the socket and send `livekit_join`, then wait for `livekit_room_info`.
  /// `connectTimeout` is a single bound covering both phases (socket open + room
  /// join). `queue_position` messages refresh the deadline, so time spent waiting
  /// in the inference queue does not count against it. @throws on failure.
  RoomInfo openAndJoin(std::chrono::milliseconds connectTimeout);

  /// Send a prompt and block until acknowledged. @throws on failure/timeout.
  void sendPrompt(const std::string& text, bool enhance, std::chrono::milliseconds timeout);

  /// Send a `set_image` (data or null) and block until acknowledged.
  void setImageData(const std::optional<std::string>& base64, const std::optional<std::string>& prompt,
                    const std::optional<bool>& enhance, std::chrono::milliseconds timeout);

  /// Send a `set_image` by server file reference and block until acknowledged.
  void setImageRef(const std::string& ref, const std::optional<std::string>& prompt,
                   const std::optional<bool>& enhance, std::chrono::milliseconds timeout);

  /// Close the socket. Idempotent.
  void close();

  bool isOpen() const;

private:
  struct Waiter {
    std::function<bool(const IncomingMessage&)> match;
    IncomingMessage result;
    bool ready = false;
  };

  void onMessage(const std::string& text);
  void onClosed(int code, const std::string& reason);
  void onSocketError(const std::string& reason);

  // Register `waiter`, send `message`, and block until matched or `timeout`.
  IncomingMessage request(const nlohmann::json& message, std::function<bool(const IncomingMessage&)> match,
                          std::chrono::milliseconds timeout, const char* label);

  bool sendText(const std::string& text);

  std::string url_;
  std::string userAgent_;
  std::unique_ptr<ix::WebSocket> ws_;
  Callbacks callbacks_;

  // Serializes request()/ack round-trips. set_image_ack carries no correlation
  // id, so only one ack-bearing request may be outstanding at a time; concurrent
  // callers queue here rather than racing to claim each other's acks.
  std::mutex requestMutex_;

  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<std::shared_ptr<Waiter>> waiters_;

  bool open_ = false;
  bool connected_ = false; // room_info received (handshake complete)
  bool closing_ = false;
  std::optional<std::string> fatalError_;

  // Handshake deadline, extendable when queue_position arrives.
  std::chrono::steady_clock::time_point handshakeDeadline_{};
  std::chrono::milliseconds handshakeWindow_{};
};

} // namespace decart::detail
