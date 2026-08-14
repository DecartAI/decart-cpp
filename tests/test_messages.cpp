// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include "realtime/messages.h"

using namespace decart::detail;

TEST_CASE("makePromptMsg produces the wire shape") {
  auto j = makePromptMsg("Anime", true);
  CHECK(j["type"] == "prompt");
  CHECK(j["prompt"] == "Anime");
  CHECK(j["enhance_prompt"] == true);
}

TEST_CASE("makeSetImageDataMsg encodes data and null clears") {
  auto with = makeSetImageDataMsg("aGVsbG8=", std::string("noir"), true);
  CHECK(with["type"] == "set_image");
  CHECK(with["image_data"] == "aGVsbG8=");
  CHECK(with["prompt"] == "noir");
  CHECK(with["enhance_prompt"] == true);
  CHECK_FALSE(with.contains("image_ref"));

  auto cleared = makeSetImageDataMsg(std::nullopt, std::nullopt, std::nullopt);
  CHECK(cleared["image_data"].is_null());
  CHECK_FALSE(cleared.contains("prompt"));
  CHECK_FALSE(cleared.contains("enhance_prompt"));
}

TEST_CASE("makeSetImageRefMsg references a server file id") {
  auto j = makeSetImageRefMsg("file_123", std::nullopt, std::nullopt);
  CHECK(j["image_ref"] == "file_123");
  CHECK_FALSE(j.contains("image_data"));
}

TEST_CASE("makeJoinMsg embeds initial state or null") {
  auto empty = makeJoinMsg(std::nullopt);
  CHECK(empty["type"] == "livekit_join");
  CHECK(empty["initial_state"].is_null());

  auto withState = makeJoinMsg(makePromptMsg("hi", false));
  CHECK(withState["initial_state"]["type"] == "prompt");
  CHECK(withState["initial_state"]["prompt"] == "hi");

  auto withBootstrap = makeJoinMsg(makeSetImageDataMsg(std::nullopt, std::nullopt, std::nullopt));
  CHECK(withBootstrap["initial_state"]["type"] == "set_image");
  CHECK(withBootstrap["initial_state"]["image_data"].is_null());
}

TEST_CASE("parseIncoming handles livekit_room_info") {
  auto m = parseIncoming(
      R"({"type":"livekit_room_info","livekit_url":"wss://lk","token":"t","room_name":"r","session_id":"s"})");
  CHECK(m.type == IncomingType::LiveKitRoomInfo);
  CHECK(m.livekitUrl == "wss://lk");
  CHECK(m.token == "t");
  CHECK(m.roomName == "r");
  CHECK(m.sessionId == "s");
}

TEST_CASE("parseIncoming defaults session_id to room_name when absent") {
  auto m =
      parseIncoming(R"({"type":"livekit_room_info","livekit_url":"u","token":"t","room_name":"room42"})");
  CHECK(m.sessionId == "room42");
}

TEST_CASE("parseIncoming handles queue, generation and acks") {
  auto q = parseIncoming(R"({"type":"queue_position","position":3,"queue_size":10})");
  CHECK(q.type == IncomingType::QueuePosition);
  CHECK(q.position == 3);
  CHECK(q.queueSize == 10);

  auto started = parseIncoming(R"({"type":"generation_started"})");
  CHECK(started.type == IncomingType::GenerationStarted);

  auto tick = parseIncoming(R"({"type":"generation_tick","seconds":1.5})");
  CHECK(tick.type == IncomingType::GenerationTick);
  CHECK(tick.seconds == doctest::Approx(1.5));

  auto ended = parseIncoming(R"({"type":"generation_ended","seconds":4,"reason":"done"})");
  CHECK(ended.type == IncomingType::GenerationEnded);
  CHECK(ended.reason == "done");

  auto pa = parseIncoming(R"({"type":"prompt_ack","prompt":"hi","success":true,"error":null})");
  CHECK(pa.type == IncomingType::PromptAck);
  CHECK(pa.prompt == "hi");
  CHECK(pa.success == true);
  CHECK_FALSE(pa.error.has_value());

  auto ia = parseIncoming(R"({"type":"set_image_ack","success":false,"error":"too big"})");
  CHECK(ia.type == IncomingType::SetImageAck);
  CHECK(ia.success == false);
  REQUIRE(ia.error.has_value());
  CHECK(*ia.error == "too big");
}

TEST_CASE("parseIncoming surfaces server errors") {
  auto e = parseIncoming(R"({"type":"error","error":"unauthorized"})");
  CHECK(e.type == IncomingType::ServerError);
  CHECK(e.errorMessage == "unauthorized");
}

TEST_CASE("parseIncoming is resilient to malformed and unknown input") {
  CHECK(parseIncoming("not json").type == IncomingType::Unknown);
  CHECK(parseIncoming("[]").type == IncomingType::Unknown);
  CHECK(parseIncoming(R"({"type":"future_thing"})").type == IncomingType::Unknown);
  CHECK(parseIncoming(R"({"no":"type"})").type == IncomingType::Unknown);
}
