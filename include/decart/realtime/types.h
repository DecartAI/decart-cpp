// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>

namespace decart {

/// Lifecycle of a realtime session, reported via `ConnectOptions::onConnectionState`.
/// Matches the `connectionChange` states across the Decart SDKs.
enum class ConnectionState {
  /// Establishing the signaling/media connection.
  Connecting,
  /// Connected; media is flowing but the model has not started generating yet.
  Connected,
  /// The model is actively generating transformed frames.
  Generating,
  /// Not connected (initial state, or after disconnect/failure).
  Disconnected,
  /// A transient drop occurred and the SDK is attempting to recover.
  Reconnecting,
};

/// Human-readable name for a ConnectionState (e.g. "generating").
const char* toString(ConnectionState state) noexcept;

/// A text prompt plus whether the server should enhance it.
struct Prompt {
  std::string text;
  bool enhance = true;
};

/// An image input. Provide exactly one of `image` / `ref`.
///
/// `image` accepts a local file path, a `data:` URL, or a raw base64 string
/// (remote http(s) URLs are not fetched — download them yourself). `ref` is a
/// server-side file id and is sent by reference instead of inlining bytes.
struct ImageInput {
  std::optional<std::string> image;
  std::optional<std::string> ref;

  static ImageInput fromPath(std::string path) { return {std::move(path), std::nullopt}; }
  static ImageInput fromBase64(std::string base64) { return {std::move(base64), std::nullopt}; }
  static ImageInput fromRef(std::string id) { return {std::nullopt, std::move(id)}; }
};

/// Initial state applied during the connection handshake, before the first
/// frame is generated.
struct InitialState {
  std::optional<Prompt> prompt;
  std::optional<ImageInput> image;
};

/// Reported while waiting in the inference queue before a session starts.
struct QueuePosition {
  int position = 0;
  int queueSize = 0;
};

/// Periodic progress signal emitted while the model generates.
struct GenerationTick {
  double seconds = 0.0;
};

/// Emitted when a generation run ends.
struct GenerationEnded {
  double seconds = 0.0;
  std::string reason;
};

} // namespace decart
