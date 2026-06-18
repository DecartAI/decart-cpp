// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/client.h"

#include <cstdlib>

#include "decart/errors.h"
#include "detail/client_config.h"
#ifndef DECART_NO_REALTIME
#include "detail/livekit_runtime.h"
#endif

namespace decart {
namespace {

std::string readEnv(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string();
}

std::string stripTrailingSlash(std::string url) {
  while (!url.empty() && url.back() == '/') url.pop_back();
  return url;
}

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

} // namespace

struct Client::Impl {
  std::shared_ptr<detail::ClientConfig> config;
#ifndef DECART_NO_REALTIME
  std::unique_ptr<RealtimeClient> realtime;
#endif
  std::unique_ptr<TokensClient> tokens;
};

Client::Client(ClientOptions options) : impl_(std::make_unique<Impl>()) {
  auto config = std::make_shared<detail::ClientConfig>();

  config->apiKey = !options.apiKey.empty() ? options.apiKey : readEnv("DECART_API_KEY");
  if (config->apiKey.empty()) {
    throw Exception(Error{ErrorCode::InvalidApiKey,
                          "Missing API key. Pass ClientOptions::apiKey or set the "
                          "DECART_API_KEY environment variable."});
  }

  std::string baseUrl = options.baseUrl.empty() ? "https://api.decart.ai" : options.baseUrl;
  if (!startsWith(baseUrl, "http://") && !startsWith(baseUrl, "https://")) {
    throw Exception(Error{ErrorCode::InvalidBaseUrl, "Invalid base URL: " + baseUrl});
  }

  std::string realtimeBaseUrl =
      options.realtimeBaseUrl.empty() ? "wss://api3.decart.ai" : options.realtimeBaseUrl;
  if (!startsWith(realtimeBaseUrl, "ws://") && !startsWith(realtimeBaseUrl, "wss://")) {
    throw Exception(Error{ErrorCode::InvalidBaseUrl,
                          "Invalid realtime base URL (must be ws:// or wss://): " + realtimeBaseUrl});
  }

  config->baseUrl = stripTrailingSlash(std::move(baseUrl));
  config->realtimeBaseUrl = stripTrailingSlash(std::move(realtimeBaseUrl));
  config->integration = options.integration;

  impl_->config = config;
  impl_->tokens = std::make_unique<TokensClient>(config);
#ifndef DECART_NO_REALTIME
  impl_->realtime = std::make_unique<RealtimeClient>(config);
  // Initialize the LiveKit runtime up front so callers can construct a
  // livekit::VideoSource before connect(). Idempotent and safe to repeat.
  detail::ensureLiveKitInit();
#endif
}

Client::~Client() = default;
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

#ifndef DECART_NO_REALTIME
RealtimeClient& Client::realtime() { return *impl_->realtime; }
#endif
TokensClient& Client::tokens() { return *impl_->tokens; }

const std::string& Client::apiKey() const noexcept { return impl_->config->apiKey; }
const std::string& Client::baseUrl() const noexcept { return impl_->config->baseUrl; }
const std::string& Client::realtimeBaseUrl() const noexcept { return impl_->config->realtimeBaseUrl; }

} // namespace decart
