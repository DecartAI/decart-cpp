// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace decart::detail {

/// Build the SDK User-Agent string, e.g.
/// "decart-sdk-cpp/0.1.0 (macos; arm64) integration/my-app".
std::string buildUserAgent(const std::string& integration = {});

} // namespace decart::detail
