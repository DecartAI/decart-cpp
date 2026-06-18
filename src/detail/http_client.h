// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <map>
#include <string>

namespace decart::detail {

struct HttpResponse {
  int status = 0;
  std::string body;
  /// Transport-level error (empty when the request reached the server).
  std::string transportError;
};

/// Minimal JSON HTTP POST used by the auth/tokens API. Thin wrapper over
/// IXWebSocket's HttpClient so the rest of the SDK stays transport-agnostic.
HttpResponse httpPostJson(const std::string& url, const std::string& body,
                          const std::map<std::string, std::string>& headers);

} // namespace decart::detail
