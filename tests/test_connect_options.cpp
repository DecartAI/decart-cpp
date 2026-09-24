// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include <chrono>
#include <optional>
#include <string>

#include "decart/models.h"
#include "decart/realtime/realtime.h"

using namespace decart;

// Compile-time guard: the field order of ConnectOptions up to `connectTimeout`
// is part of the source-compatibility contract. A positional aggregate
// initializer written against the pre-`speed` shape must still compile and
// bind every field to the same member; new options are appended after it.
TEST_CASE("ConnectOptions keeps its pre-speed positional initializer shape") {
  const ConnectOptions options{
      models::realtime("lucy-2.5"), // model
      nullptr,                      // onRemoteFrame
      nullptr,                      // onConnectionState
      nullptr,                      // onError
      nullptr,                      // onQueuePosition
      nullptr,                      // onGenerationTick
      nullptr,                      // onGenerationEnded
      InitialState{},               // initialState
      std::string("1080p"),         // resolution
      true,                         // startMuted
      std::chrono::milliseconds{5000},
  };
  CHECK(options.model.name == "lucy-2.5");
  CHECK(options.resolution == std::optional<std::string>("1080p"));
  CHECK(options.startMuted == true);
  CHECK(options.connectTimeout == std::chrono::milliseconds{5000});
  CHECK_FALSE(options.speed.has_value()); // trailing, defaulted
}

TEST_CASE("ConnectOptions::speed defaults to unset and accepts Speed::Fast") {
  ConnectOptions options;
  CHECK_FALSE(options.speed.has_value());
  options.speed = Speed::Fast;
  REQUIRE(options.speed.has_value());
  CHECK(std::string(toString(*options.speed)) == "fast");
}
