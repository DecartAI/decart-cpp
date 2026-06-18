// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include <string>

#include "decart/errors.h"
#include "decart/realtime/types.h"

using namespace decart;

TEST_CASE("ErrorCode names are stable and cross-SDK aligned") {
  CHECK(std::string(toString(ErrorCode::InvalidApiKey)) == "INVALID_API_KEY");
  CHECK(std::string(toString(ErrorCode::ModelNotFound)) == "MODEL_NOT_FOUND");
  CHECK(std::string(toString(ErrorCode::ServerError)) == "WEBRTC_SERVER_ERROR");
}

TEST_CASE("ConnectionState names match the other SDKs") {
  CHECK(std::string(toString(ConnectionState::Connecting)) == "connecting");
  CHECK(std::string(toString(ConnectionState::Connected)) == "connected");
  CHECK(std::string(toString(ConnectionState::Generating)) == "generating");
  CHECK(std::string(toString(ConnectionState::Disconnected)) == "disconnected");
  CHECK(std::string(toString(ConnectionState::Reconnecting)) == "reconnecting");
}

TEST_CASE("ImageInput factory helpers set the right field") {
  auto p = ImageInput::fromPath("/tmp/a.png");
  CHECK(p.image.has_value());
  CHECK_FALSE(p.ref.has_value());

  auto r = ImageInput::fromRef("file_1");
  CHECK(r.ref.has_value());
  CHECK_FALSE(r.image.has_value());
}
