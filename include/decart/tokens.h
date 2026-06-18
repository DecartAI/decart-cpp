// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace decart {

namespace detail {
struct ClientConfig;
} // namespace detail

/// Options for creating a short-lived client token.
struct CreateTokenOptions {
  /// Seconds until the token expires (1-3600, server default 60).
  std::optional<int> expiresIn;
  /// Custom key/value pairs attached to the token.
  std::map<std::string, std::string> metadata;
  /// Restrict which models the token may access (max 20).
  std::vector<std::string> allowedModels;
  /// Restrict which web origins the token may be used from (max 20).
  std::vector<std::string> allowedOrigins;
  /// Optional cap on realtime session duration, in seconds.
  std::optional<int> maxRealtimeSessionDuration;
};

/// Result of creating a client token.
struct CreateTokenResponse {
  /// Short-lived API key (prefixed "ek_"), safe for client-side use.
  std::string apiKey;
  /// ISO-8601 expiry timestamp.
  std::string expiresAt;
  /// Raw JSON response body, for fields not surfaced as typed members.
  std::string rawJson;
};

/// Client for creating short-lived client tokens. Obtained from `Client::tokens()`.
class TokensClient {
public:
  /// Create a client token. @throws decart::Exception on failure.
  CreateTokenResponse create(const CreateTokenOptions& options = {});

  /// @internal Constructed by `Client`; not intended for direct use.
  explicit TokensClient(std::shared_ptr<detail::ClientConfig> config);

private:
  std::shared_ptr<detail::ClientConfig> config_;
};

} // namespace decart
