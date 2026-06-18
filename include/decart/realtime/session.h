// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "decart/realtime/types.h"

namespace decart {

namespace detail {
struct SessionInternals; // grants the realtime orchestrator access to Impl
} // namespace detail

/// Combined update for `RealtimeSession::set` — change the prompt, the reference
/// image, or both in a single call. At least one of `prompt`/`image` must be set.
struct SetInput {
  std::optional<std::string> prompt;
  bool enhance = true;
  std::optional<ImageInput> image;
};

/// Per-call options for image/prompt updates.
struct UpdateOptions {
  /// Optional prompt to apply alongside an image update.
  std::optional<std::string> prompt;
  bool enhance = true;
  /// How long to wait for the server acknowledgement before timing out.
  std::chrono::milliseconds timeout{30000};
};

/// A live realtime session. Created by `RealtimeClient::connect`.
///
/// Methods that talk to the server (`setPrompt`, `set`, `setImage`) block until
/// the server acknowledges and throw `decart::Exception` on failure or timeout.
/// The object is move-only; destroying it disconnects.
///
/// Thread-safety: control methods may be called from any thread. They must not
/// be called from inside a `ConnectOptions` callback.
class RealtimeSession {
public:
  RealtimeSession();
  ~RealtimeSession();

  RealtimeSession(const RealtimeSession&) = delete;
  RealtimeSession& operator=(const RealtimeSession&) = delete;
  RealtimeSession(RealtimeSession&&) noexcept;
  RealtimeSession& operator=(RealtimeSession&&) noexcept;

  /// Set the active text prompt. Blocks until acknowledged.
  void setPrompt(const std::string& prompt, bool enhance = true);

  /// Apply a prompt and/or reference image in one call. Blocks until acknowledged.
  void set(const SetInput& input);

  /// Set (or clear) the reference image. Pass `std::nullopt` to clear.
  /// Blocks until acknowledged.
  void setImage(const std::optional<ImageInput>& image, const UpdateOptions& options = {});

  /// True while the session is connected (state `Connected` or `Generating`).
  bool isConnected() const noexcept;

  /// Current connection state.
  ConnectionState connectionState() const noexcept;

  /// Server session id, available once the session has started.
  std::optional<std::string> sessionId() const;

  /// Tear down the session. Idempotent. Safe to call from any thread except
  /// from inside a session callback.
  void disconnect();

private:
  friend struct detail::SessionInternals;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace decart
