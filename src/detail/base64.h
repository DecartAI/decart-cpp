// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace decart::detail {

/// Standard base64 encode (RFC 4648, with padding).
std::string base64Encode(const std::uint8_t* data, std::size_t size);
std::string base64Encode(const std::vector<std::uint8_t>& data);
std::string base64Encode(const std::string& data);

} // namespace decart::detail
