// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "detail/base64.h"

using decart::detail::base64Encode;

static std::string enc(const std::string& s) {
  return base64Encode(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

TEST_CASE("base64Encode matches RFC 4648 test vectors") {
  CHECK(enc("") == "");
  CHECK(enc("f") == "Zg==");
  CHECK(enc("fo") == "Zm8=");
  CHECK(enc("foo") == "Zm9v");
  CHECK(enc("foob") == "Zm9vYg==");
  CHECK(enc("fooba") == "Zm9vYmE=");
  CHECK(enc("foobar") == "Zm9vYmFy");
}

TEST_CASE("base64Encode handles binary bytes") {
  std::vector<std::uint8_t> bytes{0x00, 0xFF, 0x10, 0x80, 0x7F};
  CHECK(base64Encode(bytes) == "AP8QgH8=");
}
