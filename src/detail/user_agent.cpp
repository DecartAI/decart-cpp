// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "detail/user_agent.h"

#include "decart/version.h"

namespace decart::detail {
namespace {

constexpr const char* osName() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

constexpr const char* archName() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x64";
#else
  return "unknown";
#endif
}

} // namespace

std::string buildUserAgent(const std::string& integration) {
  std::string ua = "decart-sdk-cpp/" DECART_SDK_VERSION " (";
  ua += osName();
  ua += "; ";
  ua += archName();
  ua += ")";
  if (!integration.empty()) {
    ua += " integration/";
    ua += integration;
  }
  return ua;
}

} // namespace decart::detail
