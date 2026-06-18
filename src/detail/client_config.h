// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace decart::detail {

/// Resolved client configuration shared by the top-level `Client` and its
/// sub-clients (realtime, tokens).
struct ClientConfig {
  std::string apiKey;
  std::string baseUrl;         // e.g. "https://api.decart.ai"
  std::string realtimeBaseUrl; // e.g. "wss://api3.decart.ai"
  std::string integration;
};

} // namespace decart::detail
