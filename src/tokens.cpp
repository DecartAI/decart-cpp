// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/tokens.h"

#include <nlohmann/json.hpp>

#include "decart/errors.h"
#include "detail/client_config.h"
#include "detail/http_client.h"
#include "detail/user_agent.h"

namespace decart {

TokensClient::TokensClient(std::shared_ptr<detail::ClientConfig> config) : config_(std::move(config)) {}

CreateTokenResponse TokensClient::create(const CreateTokenOptions& options) {
  using nlohmann::json;

  json body = json::object();
  if (options.expiresIn.has_value()) body["expiresIn"] = *options.expiresIn;
  if (!options.metadata.empty()) body["metadata"] = options.metadata;
  if (!options.allowedModels.empty()) body["allowedModels"] = options.allowedModels;
  if (!options.allowedOrigins.empty()) body["allowedOrigins"] = options.allowedOrigins;
  if (options.maxRealtimeSessionDuration.has_value()) {
    body["constraints"] = {{"realtime", {{"maxSessionDuration", *options.maxRealtimeSessionDuration}}}};
  }

  const std::string url = config_->baseUrl + "/v1/client/tokens";
  const std::map<std::string, std::string> headers{
      {"X-API-KEY", config_->apiKey},
      {"Content-Type", "application/json"},
      {"User-Agent", detail::buildUserAgent(config_->integration)},
  };

  const detail::HttpResponse resp = detail::httpPostJson(url, body.dump(), headers);

  if (!resp.transportError.empty()) {
    throw Exception(Error{ErrorCode::HttpError, "Token request failed: " + resp.transportError});
  }
  if (resp.status < 200 || resp.status >= 300) {
    throw Exception(Error{ErrorCode::TokenCreateError,
                          "Failed to create token: " + std::to_string(resp.status) + " - " + resp.body,
                          resp.status});
  }

  json parsed = json::parse(resp.body, nullptr, /*allow_exceptions=*/false);
  if (parsed.is_discarded() || !parsed.is_object()) {
    throw Exception(Error{ErrorCode::TokenCreateError, "Malformed token response", resp.status});
  }

  CreateTokenResponse out;
  out.apiKey = parsed.value("apiKey", std::string());
  out.expiresAt = parsed.value("expiresAt", std::string());
  out.rawJson = resp.body;
  return out;
}

} // namespace decart
