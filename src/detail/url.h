// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <cctype>
#include <string>

namespace decart::detail {

/// Percent-encode a string for use as a URL query-component value (RFC 3986
/// unreserved characters pass through).
inline std::string urlEncode(const std::string& value) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() * 3);
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

/// Append `key=urlEncode(value)` to `url`, choosing `?` or `&`.
inline std::string appendQuery(const std::string& url, const std::string& key, const std::string& value) {
  const char sep = url.find('?') == std::string::npos ? '?' : '&';
  return url + sep + key + "=" + urlEncode(value);
}

} // namespace decart::detail
