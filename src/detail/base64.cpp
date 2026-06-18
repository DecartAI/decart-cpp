// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "detail/base64.h"

namespace decart::detail {
namespace {
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string base64Encode(const std::uint8_t* data, std::size_t size) {
  std::string out;
  out.reserve(((size + 2) / 3) * 4);

  std::size_t i = 0;
  for (; i + 3 <= size; i += 3) {
    const std::uint32_t n =
        (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8) | std::uint32_t(data[i + 2]);
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back(kAlphabet[(n >> 6) & 0x3F]);
    out.push_back(kAlphabet[n & 0x3F]);
  }

  const std::size_t rem = size - i;
  if (rem == 1) {
    const std::uint32_t n = std::uint32_t(data[i]) << 16;
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (rem == 2) {
    const std::uint32_t n = (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8);
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back(kAlphabet[(n >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

std::string base64Encode(const std::vector<std::uint8_t>& data) {
  return base64Encode(data.data(), data.size());
}

std::string base64Encode(const std::string& data) {
  return base64Encode(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

} // namespace decart::detail
