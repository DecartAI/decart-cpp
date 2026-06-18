// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <stdexcept>
#include <string>

namespace decart {

/// Stable error codes surfaced by the SDK. These mirror the codes used by the
/// other Decart SDKs so behaviour can be reasoned about across languages.
enum class ErrorCode {
  /// No API key was provided and `DECART_API_KEY` was not set.
  InvalidApiKey,
  /// A configured base URL was malformed.
  InvalidBaseUrl,
  /// A caller-provided argument was invalid.
  InvalidInput,
  /// The requested model is not known to the SDK registry.
  ModelNotFound,
  /// Creating a client token failed.
  TokenCreateError,
  /// The realtime signaling WebSocket failed.
  WebsocketError,
  /// A request to the server timed out.
  TimeoutError,
  /// The server returned an explicit error message.
  ServerError,
  /// A realtime signaling-level error occurred.
  SignalingError,
  /// The realtime media (LiveKit/WebRTC) transport failed.
  MediaError,
  /// An operation requires an active connection that is not present.
  NotConnected,
  /// An HTTP request failed (network or non-2xx response).
  HttpError,
};

/// Human-readable name for an ErrorCode (e.g. "INVALID_API_KEY").
const char* toString(ErrorCode code) noexcept;

/// Structured description of an error. Carried by `Exception` and passed to the
/// `onError` realtime callback.
struct Error {
  ErrorCode code;
  std::string message;
  /// Optional HTTP status code when `code == HttpError`/`TokenCreateError`.
  int status = 0;
};

/// Exception type thrown by the SDK for synchronous failures (e.g. a failed
/// connect, a rejected prompt, or a token creation error).
class Exception : public std::runtime_error {
public:
  explicit Exception(Error error) : std::runtime_error(error.message), error_(std::move(error)) {}

  /// The structured error that caused this exception.
  const Error& error() const noexcept { return error_; }
  /// Convenience accessor for the error code.
  ErrorCode code() const noexcept { return error_.code; }

private:
  Error error_;
};

} // namespace decart
