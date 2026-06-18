// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>

#include "decart/tokens.h"
#ifndef DECART_NO_REALTIME
#include "decart/realtime/realtime.h"
#endif

namespace decart {

/// Configuration for a `Client`.
struct ClientOptions {
  /// API key for authentication. If empty, the `DECART_API_KEY` environment
  /// variable is used. Realtime requires a key.
  std::string apiKey;
  /// Override the HTTP API base URL (default "https://api.decart.ai").
  std::string baseUrl;
  /// Override the realtime signaling base URL (default "wss://api3.decart.ai").
  std::string realtimeBaseUrl;
  /// Optional integration identifier, included in the User-Agent.
  std::string integration;
};

/// Top-level Decart client. Construct once and reuse; sub-clients
/// (`realtime()`, `tokens()`) share its configuration.
///
/// @example
/// @code
/// decart::Client client; // reads DECART_API_KEY
/// auto token = client.tokens().create();
/// auto model = decart::models::realtime("lucy-restyle-2");
/// auto source = std::make_shared<livekit::VideoSource>(model.width, model.height);
/// decart::ConnectOptions opts;
/// opts.model = model;
/// opts.onRemoteFrame = [](const livekit::VideoFrame&, std::int64_t) {};
/// auto session = client.realtime().connect(source, opts);
/// @endcode
class Client {
public:
  /// Construct a client. @throws decart::Exception(InvalidApiKey) when no key is
  /// available, or (InvalidBaseUrl) when a provided URL is malformed.
  explicit Client(ClientOptions options = {});
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) noexcept;
  Client& operator=(Client&&) noexcept;

#ifndef DECART_NO_REALTIME
  /// Realtime video transformation client. Available unless the SDK was built
  /// with `DECART_BUILD_REALTIME=OFF` (auth/tokens-only).
  RealtimeClient& realtime();
#endif

  /// Client-token (auth) client.
  TokensClient& tokens();

  /// The resolved API key in use.
  const std::string& apiKey() const noexcept;
  /// The resolved HTTP API base URL.
  const std::string& baseUrl() const noexcept;
  /// The resolved realtime signaling base URL.
  const std::string& realtimeBaseUrl() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace decart
