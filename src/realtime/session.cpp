// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Realtime session orchestration: the Decart signaling handshake (control) plus
// the LiveKit room (media). See the protocol notes in [[decart-realtime-wire-protocol]].
#include "decart/realtime/session.h"

#include <livekit/livekit.h>

#include <atomic>
#include <fstream>
#include <mutex>
#include <thread>

#include "decart/errors.h"
#include "decart/logging.h"
#include "decart/realtime/realtime.h"
#include "detail/base64.h"
#include "detail/client_config.h"
#include "detail/livekit_runtime.h"
#include "detail/log.h"
#include "detail/signaling_channel.h"
#include "detail/url.h"
#include "detail/user_agent.h"
#include "realtime/messages.h"
#include "realtime/session_internal.h"

namespace decart {
namespace detail {

void ensureLiveKitInit() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    livekit::LogLevel lk = livekit::LogLevel::Warn;
    switch (decart::logLevel()) {
      case LogLevel::Trace:
        lk = livekit::LogLevel::Trace;
        break;
      case LogLevel::Debug:
        lk = livekit::LogLevel::Debug;
        break;
      case LogLevel::Info:
        lk = livekit::LogLevel::Info;
        break;
      case LogLevel::Warn:
        lk = livekit::LogLevel::Warn;
        break;
      case LogLevel::Error:
        lk = livekit::LogLevel::Error;
        break;
      case LogLevel::Off:
        lk = livekit::LogLevel::Off;
        break;
    }
    livekit::initialize(lk);
  });
}

namespace {

constexpr const char* kInferencePrefix = "inference-server-";
constexpr auto kPromptTimeout = std::chrono::milliseconds(15000);
constexpr auto kUpdateTimeout = std::chrono::milliseconds(30000);

// Resolve an image input to base64. Supports `data:` URLs, local file paths, and
// raw base64 strings. Remote http(s) URLs are not fetched in v0.1.
std::string resolveImageBase64(const std::string& image) {
  if (image.rfind("data:", 0) == 0) {
    const auto comma = image.find(',');
    return comma == std::string::npos ? image : image.substr(comma + 1);
  }
  if (image.rfind("http://", 0) == 0 || image.rfind("https://", 0) == 0) {
    throw Exception(Error{ErrorCode::InvalidInput,
                          "Remote image URLs are not supported; pass a local file path, a data: "
                          "URL, or base64 bytes."});
  }
  std::ifstream file(image, std::ios::binary);
  if (file) {
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return base64Encode(bytes);
  }
  // Not a file — treat as raw base64 (mirrors the js/python SDK fallback).
  return image;
}

} // namespace

/// Shared media half of a session: owns the LiveKit room, maps its events to the
/// SDK connection-state model, and pumps the inference-server video track to the
/// user's frame callback. Used by both publishing and subscribing sessions.
class MediaSession : public livekit::RoomDelegate {
public:
  RemoteFrameCallback onRemoteFrame;
  std::function<void(ConnectionState)> onConnectionState;
  std::function<void(const Error&)> onError;

  ~MediaSession() override { disconnect(); }

  void markIntentional() { intentional_.store(true); }

  ConnectionState state() const {
    std::lock_guard<std::mutex> lk(stateMutex_);
    return state_;
  }

  bool isConnected() const {
    const auto s = state();
    return s == ConnectionState::Connected || s == ConnectionState::Generating;
  }

  void setState(ConnectionState next) {
    std::function<void(ConnectionState)> cb;
    {
      std::lock_guard<std::mutex> lk(stateMutex_);
      if (state_ == next) return;
      state_ = next;
      cb = onConnectionState;
    }
    if (cb) cb(next);
  }

  // Transition only when currently in `from` (used for the connected<->generating
  // sub-states so external Disconnected/Reconnecting transitions are not clobbered).
  void transitionIf(ConnectionState from, ConnectionState to) {
    std::function<void(ConnectionState)> cb;
    {
      std::lock_guard<std::mutex> lk(stateMutex_);
      if (state_ != from) return;
      state_ = to;
      cb = onConnectionState;
    }
    if (cb) cb(to);
  }

  void markGenerating() { transitionIf(ConnectionState::Connected, ConnectionState::Generating); }

  // Generation finished; fall back to Connected (a new run re-enters Generating).
  void markGenerationEnded() { transitionIf(ConnectionState::Generating, ConnectionState::Connected); }

  void connectRoom(const RoomInfo& info, const std::shared_ptr<livekit::VideoSource>& source,
                   int publishFps) {
    room_ = std::make_unique<livekit::Room>();
    room_->setDelegate(this);

    livekit::RoomOptions options;
    options.auto_subscribe = true; // required to receive the inference output track
    options.dynacast = false;
    // Use dual peer connections (separate publish/subscribe), matching the
    // browser/JS SDK. Single-PC mode dropped the inference output subscription.
    options.single_peer_connection = false;

    if (!room_->connect(info.livekitUrl, info.token, options)) {
      throw Exception(Error{ErrorCode::MediaError, "Failed to connect to the LiveKit room"});
    }

    if (source) {
      auto local = room_->localParticipant().lock();
      if (!local) throw Exception(Error{ErrorCode::MediaError, "Local participant unavailable"});

      auto track = livekit::LocalVideoTrack::createLocalVideoTrack("decart-input", source);
      livekit::TrackPublishOptions publish;
      publish.source = livekit::TrackSource::SOURCE_CAMERA;
      publish.simulcast = false;
      livekit::VideoEncodingOptions encoding;
      encoding.max_bitrate = 3'500'000;
      encoding.max_framerate = static_cast<double>(publishFps);
      publish.video_encoding = encoding;
      local->publishTrack(track, publish);
      publishedTrack_ = std::move(track);
    }
  }

  void disconnect() {
    intentional_.store(true);

    if (room_) {
      {
        std::lock_guard<std::mutex> lk(mediaMutex_);
        if (videoCallbackSet_) {
          room_->clearOnVideoFrameCallback(remoteIdentity_, remoteTrackName_);
          videoCallbackSet_ = false;
        }
      }
      room_->setDelegate(nullptr);
      room_->disconnect();
      room_.reset();
    }
    publishedTrack_.reset();
    setState(ConnectionState::Disconnected);
  }

  // --- RoomDelegate ---

  void onTrackSubscribed(livekit::Room& room, const livekit::TrackSubscribedEvent& ev) override {
    const std::string identity = ev.participant ? ev.participant->identity() : "<null>";
    const bool isVideo = ev.track && ev.track->kind() == livekit::TrackKind::KIND_VIDEO;
    log(LogLevel::Debug, "trackSubscribed: identity=" + identity + " kind=" + (isVideo ? "video" : "other") +
                             " name=" + (ev.track ? ev.track->name() : std::string("<null>")));
    if (ev.participant == nullptr || !ev.track || !isVideo) return;
    if (identity.rfind(kInferencePrefix, 0) != 0) return;

    // Register a dispatcher-managed frame callback keyed by (identity, trackName).
    // The inference output track is re-published as generation (re)starts; the
    // dispatcher re-attaches a reader on each re-subscribe, where a one-shot
    // VideoStream bound to a single track handle would go silent.
    std::lock_guard<std::mutex> lk(mediaMutex_);
    if (videoCallbackSet_) return;
    videoCallbackSet_ = true;
    remoteIdentity_ = identity;
    remoteTrackName_ = ev.track->name();
    auto cb = onRemoteFrame;
    livekit::VideoStream::Options opts;
    opts.format = livekit::VideoBufferType::RGBA;
    room.setOnVideoFrameCallback(
        remoteIdentity_, remoteTrackName_,
        [cb](const livekit::VideoFrame& frame, std::int64_t ts) {
          if (cb) cb(frame, ts);
        },
        opts);
    log(LogLevel::Debug, "registered video frame callback for " + identity);
  }

  // Media-layer reconnects must not resurrect a session whose control channel
  // already died (signaling close sets Disconnected, which is terminal in v0.1),
  // so only move between the live connected<->reconnecting sub-states.
  void onReconnecting(livekit::Room&, const livekit::ReconnectingEvent&) override {
    transitionIf(ConnectionState::Connected, ConnectionState::Reconnecting);
    transitionIf(ConnectionState::Generating, ConnectionState::Reconnecting);
  }

  void onReconnected(livekit::Room&, const livekit::ReconnectedEvent&) override {
    transitionIf(ConnectionState::Reconnecting, ConnectionState::Connected);
  }

  void onDisconnected(livekit::Room&, const livekit::DisconnectedEvent&) override {
    if (intentional_.load()) return;
    setState(ConnectionState::Disconnected);
    if (onError) onError(Error{ErrorCode::MediaError, "LiveKit room disconnected"});
  }

  void onTrackSubscriptionFailed(livekit::Room&, const livekit::TrackSubscriptionFailedEvent& ev) override {
    if (onError) onError(Error{ErrorCode::MediaError, "Track subscription failed: " + ev.error});
  }

private:
  std::unique_ptr<livekit::Room> room_;
  std::shared_ptr<livekit::LocalVideoTrack> publishedTrack_;

  mutable std::mutex stateMutex_;
  ConnectionState state_ = ConnectionState::Disconnected;
  std::atomic<bool> intentional_{false};

  std::mutex mediaMutex_;
  bool videoCallbackSet_ = false;
  std::string remoteIdentity_;
  std::string remoteTrackName_;
};

} // namespace detail

// ---------------------------------------------------------------------------
// RealtimeSession
// ---------------------------------------------------------------------------

struct RealtimeSession::Impl {
  std::shared_ptr<detail::ClientConfig> config;
  std::shared_ptr<livekit::VideoSource> source;
  detail::MediaSession media;
  // Declared last so it is destroyed first: closing the socket stops callbacks
  // before `media` is torn down.
  std::unique_ptr<detail::SignalingChannel> signaling;

  mutable std::mutex infoMutex;
  std::optional<std::string> sessionId;

  void setSessionId(std::string sid) {
    std::lock_guard<std::mutex> lk(infoMutex);
    sessionId = std::move(sid);
  }

  void assertConnected() const {
    if (!media.isConnected()) {
      throw Exception(Error{ErrorCode::NotConnected, "Realtime session is not connected"});
    }
  }

  void disconnect() {
    media.markIntentional();
    if (signaling) signaling->close();
    media.disconnect();
  }
};

RealtimeSession::RealtimeSession() : impl_(std::make_unique<Impl>()) {}
RealtimeSession::~RealtimeSession() {
  if (impl_) impl_->disconnect();
}
RealtimeSession::RealtimeSession(RealtimeSession&&) noexcept = default;
RealtimeSession& RealtimeSession::operator=(RealtimeSession&&) noexcept = default;

void RealtimeSession::setPrompt(const std::string& prompt, bool enhance) {
  if (prompt.empty()) throw Exception(Error{ErrorCode::InvalidInput, "Prompt cannot be empty"});
  impl_->assertConnected();
  impl_->signaling->sendPrompt(prompt, enhance, detail::kPromptTimeout);
}

void RealtimeSession::set(const SetInput& input) {
  if (!input.prompt.has_value() && !input.image.has_value()) {
    throw Exception(Error{ErrorCode::InvalidInput, "At least one of 'prompt' or 'image' must be provided"});
  }
  impl_->assertConnected();

  // Prompt-only update: send a prompt message so the active reference image is
  // preserved (a set_image with image_data:null would clear it). Use setImage()
  // to change or clear the image explicitly.
  if (!input.image.has_value()) {
    impl_->signaling->sendPrompt(*input.prompt, input.enhance, detail::kPromptTimeout);
    return;
  }

  const std::optional<std::string> prompt = input.prompt;
  const std::optional<bool> enhance =
      input.prompt.has_value() ? std::optional<bool>(input.enhance) : std::nullopt;

  if (input.image->ref.has_value()) {
    impl_->signaling->setImageRef(*input.image->ref, prompt, enhance, detail::kUpdateTimeout);
    return;
  }
  std::optional<std::string> base64;
  if (input.image->image.has_value()) {
    base64 = detail::resolveImageBase64(*input.image->image);
  }
  impl_->signaling->setImageData(base64, prompt, enhance, detail::kUpdateTimeout);
}

void RealtimeSession::setImage(const std::optional<ImageInput>& image, const UpdateOptions& options) {
  impl_->assertConnected();
  const std::optional<std::string> prompt = options.prompt;
  const std::optional<bool> enhance =
      options.prompt.has_value() ? std::optional<bool>(options.enhance) : std::nullopt;

  if (image.has_value() && image->ref.has_value()) {
    impl_->signaling->setImageRef(*image->ref, prompt, enhance, options.timeout);
    return;
  }
  std::optional<std::string> base64;
  if (image.has_value() && image->image.has_value()) {
    base64 = detail::resolveImageBase64(*image->image);
  }
  impl_->signaling->setImageData(base64, prompt, enhance, options.timeout);
}

bool RealtimeSession::isConnected() const noexcept { return impl_->media.isConnected(); }
ConnectionState RealtimeSession::connectionState() const noexcept { return impl_->media.state(); }

std::optional<std::string> RealtimeSession::sessionId() const {
  std::lock_guard<std::mutex> lk(impl_->infoMutex);
  return impl_->sessionId;
}

void RealtimeSession::disconnect() { impl_->disconnect(); }

// ---------------------------------------------------------------------------
// Orchestration (declared in session_internal.h)
// ---------------------------------------------------------------------------

namespace detail {

// Friend of RealtimeSession; lets the orchestrator reach Impl.
struct SessionInternals {
  static RealtimeSession::Impl* impl(RealtimeSession& session) { return session.impl_.get(); }
};

std::unique_ptr<RealtimeSession> connectRealtime(std::shared_ptr<ClientConfig> config,
                                                 std::shared_ptr<livekit::VideoSource> source,
                                                 ConnectOptions options) {
  if (!source) {
    throw Exception(Error{ErrorCode::InvalidInput,
                          "connect() requires a video source to publish; pass a livekit::VideoSource"});
  }
  ensureLiveKitInit();

  auto session = std::unique_ptr<RealtimeSession>(new RealtimeSession());
  auto* impl = SessionInternals::impl(*session);
  impl->config = config;
  impl->source = source;
  impl->media.onRemoteFrame = options.onRemoteFrame;
  impl->media.onConnectionState = options.onConnectionState;
  impl->media.onError = options.onError;
  impl->media.setState(ConnectionState::Connecting);

  std::string url = config->realtimeBaseUrl + options.model.urlPath;
  url = appendQuery(url, "api_key", config->apiKey);
  url = appendQuery(url, "model", options.model.name);
  if (options.resolution.has_value()) url = appendQuery(url, "resolution", *options.resolution);

  impl->signaling = std::make_unique<SignalingChannel>(url, buildUserAgent(config->integration));

  SignalingChannel::Callbacks callbacks;
  callbacks.onQueuePosition = [cb = options.onQueuePosition](int position, int size) {
    if (cb) cb(QueuePosition{position, size});
  };
  callbacks.onGenerationStarted = [impl] { impl->media.markGenerating(); };
  callbacks.onGenerationTick = [impl, cb = options.onGenerationTick](double seconds) {
    impl->media.markGenerating();
    if (cb) cb(GenerationTick{seconds});
  };
  callbacks.onGenerationEnded = [impl, cb = options.onGenerationEnded](double seconds,
                                                                       const std::string& reason) {
    impl->media.markGenerationEnded();
    if (cb) cb(GenerationEnded{seconds, reason});
  };
  callbacks.onServerError = [cb = options.onError](const std::string& message) {
    if (cb) cb(Error{ErrorCode::ServerError, message});
  };
  // An unexpected signaling drop after connect means control is dead: reflect it
  // in the connection state so isConnected() stops returning true, then notify.
  callbacks.onClosed = [impl, cb = options.onError](int code, const std::string& reason) {
    impl->media.setState(ConnectionState::Disconnected);
    if (cb)
      cb(Error{ErrorCode::WebsocketError,
               "Signaling channel closed: " + std::to_string(code) + " " + reason});
  };
  impl->signaling->setCallbacks(std::move(callbacks));

  try {
    const RoomInfo info = impl->signaling->openAndJoin(options.connectTimeout);

    // Initial reference image / prompt rides ahead of the media connect so the
    // model has it before the first frame (mirrors the python SDK ordering).
    bool hasInitialState = false;
    const auto& initial = options.initialState;
    const std::optional<std::string> initialPrompt =
        initial.prompt.has_value() ? std::optional<std::string>(initial.prompt->text) : std::nullopt;
    const std::optional<bool> initialEnhance =
        initial.prompt.has_value() ? std::optional<bool>(initial.prompt->enhance) : std::nullopt;

    if (initial.image.has_value()) {
      hasInitialState = true;
      if (initial.image->ref.has_value()) {
        impl->signaling->setImageRef(*initial.image->ref, initialPrompt, initialEnhance, kUpdateTimeout);
      } else if (initial.image->image.has_value()) {
        impl->signaling->setImageData(resolveImageBase64(*initial.image->image), initialPrompt,
                                      initialEnhance, kUpdateTimeout);
      }
    } else if (initial.prompt.has_value()) {
      hasInitialState = true;
      impl->signaling->sendPrompt(initial.prompt->text, initial.prompt->enhance, kPromptTimeout);
    }

    impl->media.connectRoom(info, source, options.model.fps);

    // With no explicit initial state, a passthrough set_image kicks off generation.
    if (!hasInitialState) {
      impl->signaling->setImageData(std::nullopt, std::nullopt, std::nullopt, kUpdateTimeout);
    }

    impl->setSessionId(info.sessionId);
    impl->media.setState(ConnectionState::Connected);
    return session;
  } catch (...) {
    impl->disconnect();
    throw;
  }
}

} // namespace detail
} // namespace decart
