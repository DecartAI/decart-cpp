// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "decart/errors.h"
#include "decart/models.h"
#include "decart/realtime/session.h"
#include "decart/realtime/types.h"

// The realtime API is built on the official LiveKit C++ SDK. Consumers create
// and drive a `livekit::VideoSource`; the SDK publishes it and delivers the
// transformed frames back through `onRemoteFrame`. Forward-declared here so that
// translation units which only use auth/tokens need not include LiveKit headers.
namespace livekit {
class VideoSource;
class VideoFrame;
} // namespace livekit

namespace decart {

namespace detail {
struct ClientConfig;
struct SessionInternals;
} // namespace detail

/// Callback delivering each transformed frame from the model. Invoked on a
/// dedicated reader thread; keep it fast and do not call session methods from it.
using RemoteFrameCallback = std::function<void(const livekit::VideoFrame& frame, std::int64_t timestamp_us)>;

/// Options for `RealtimeClient::connect`.
struct ConnectOptions {
  /// Model to run. Use `decart::models::realtime(...)`.
  ModelDefinition model;

  /// Receives transformed frames from the model. Required to see output.
  RemoteFrameCallback onRemoteFrame;

  /// Notified on every connection-state transition.
  std::function<void(ConnectionState)> onConnectionState;

  /// Notified on asynchronous, non-fatal errors during the session.
  std::function<void(const Error&)> onError;

  /// Notified while waiting in the inference queue.
  std::function<void(const QueuePosition&)> onQueuePosition;

  /// Notified periodically while the model generates.
  std::function<void(const GenerationTick&)> onGenerationTick;

  /// Notified when a generation run ends.
  std::function<void(const GenerationEnded&)> onGenerationEnded;

  /// Optional prompt/image to apply during the handshake.
  InitialState initialState;

  /// Optional output resolution hint ("720p" or "1080p").
  std::optional<std::string> resolution;

  /// Publish the input track muted, so no frames reach the model until you call
  /// `RealtimeSession::unmute()`. Use this to pre-warm a connection: the session
  /// is fully authenticated and the WebRTC media path is established, but nothing
  /// is transmitted — and no generation is billed — while muted. An idle video
  /// track still emits keepalive frames over WebRTC, so muting (rather than just
  /// withholding `captureFrame`) is what keeps a warmed session free. Pair with
  /// `initialState` to apply the prompt/image during the idle warmup phase.
  bool startMuted = false;

  /// Bound on the signaling handshake (socket open + room join). The LiveKit
  /// media connect manages its own timeout. Default 60s.
  std::chrono::milliseconds connectTimeout{60000};
};

/// Entry point for realtime video transformation. Obtained from `Client::realtime()`.
class RealtimeClient {
public:
  /// Connect a realtime session that publishes frames from `source` and
  /// delivers transformed frames via `options.onRemoteFrame`.
  ///
  /// The caller owns `source` and pushes frames into it (via
  /// `livekit::VideoSource::captureFrame`) for the lifetime of the session.
  /// `source` is required (must be non-null).
  ///
  /// Blocks until connected. @throws decart::Exception on failure (including
  /// `InvalidInput` if `source` is null).
  std::unique_ptr<RealtimeSession> connect(std::shared_ptr<livekit::VideoSource> source,
                                           ConnectOptions options);

  /// @internal Constructed by `Client`; not intended for direct use.
  explicit RealtimeClient(std::shared_ptr<detail::ClientConfig> config);

private:
  std::shared_ptr<detail::ClientConfig> config_;
};

} // namespace decart
