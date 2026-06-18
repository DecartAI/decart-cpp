// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "realtime/messages.h"

namespace decart::detail {
namespace {

using nlohmann::json;

void applyPromptFields(json& msg, const std::optional<std::string>& prompt,
                       const std::optional<bool>& enhance) {
  if (prompt.has_value()) msg["prompt"] = *prompt;
  if (enhance.has_value()) msg["enhance_prompt"] = *enhance;
}

// nlohmann throws on type mismatch; value() with a default tolerates absent /
// null / wrong-typed fields, which keeps parsing resilient to server changes.
template <typename T>
T get(const json& j, const char* key, T fallback) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return fallback;
  try {
    return it->get<T>();
  } catch (const json::exception&) {
    return fallback;
  }
}

} // namespace

nlohmann::json makePromptMsg(const std::string& text, bool enhance) {
  return json{{"type", "prompt"}, {"prompt", text}, {"enhance_prompt", enhance}};
}

nlohmann::json makeSetImageDataMsg(const std::optional<std::string>& base64,
                                   const std::optional<std::string>& prompt,
                                   const std::optional<bool>& enhance) {
  json msg{{"type", "set_image"}};
  if (base64.has_value()) {
    msg["image_data"] = *base64;
  } else {
    msg["image_data"] = nullptr;
  }
  applyPromptFields(msg, prompt, enhance);
  return msg;
}

nlohmann::json makeSetImageRefMsg(const std::string& ref, const std::optional<std::string>& prompt,
                                  const std::optional<bool>& enhance) {
  json msg{{"type", "set_image"}, {"image_ref", ref}};
  applyPromptFields(msg, prompt, enhance);
  return msg;
}

nlohmann::json makeJoinMsg(const std::optional<nlohmann::json>& initialState) {
  json msg{{"type", "livekit_join"}};
  msg["initial_state"] = initialState.has_value() ? *initialState : json(nullptr);
  return msg;
}

IncomingMessage parseIncoming(const std::string& text) {
  IncomingMessage out;
  json j = json::parse(text, /*cb=*/nullptr, /*allow_exceptions=*/false);
  if (j.is_discarded() || !j.is_object()) return out;

  const std::string type = get<std::string>(j, "type", "");
  if (type == "livekit_room_info") {
    out.type = IncomingType::LiveKitRoomInfo;
    out.livekitUrl = get<std::string>(j, "livekit_url", "");
    out.token = get<std::string>(j, "token", "");
    out.roomName = get<std::string>(j, "room_name", "");
    out.sessionId = get<std::string>(j, "session_id", out.roomName);
  } else if (type == "queue_position") {
    out.type = IncomingType::QueuePosition;
    out.position = get<int>(j, "position", 0);
    out.queueSize = get<int>(j, "queue_size", 0);
  } else if (type == "generation_started") {
    out.type = IncomingType::GenerationStarted;
  } else if (type == "generation_tick") {
    out.type = IncomingType::GenerationTick;
    out.seconds = get<double>(j, "seconds", 0.0);
  } else if (type == "generation_ended") {
    out.type = IncomingType::GenerationEnded;
    out.seconds = get<double>(j, "seconds", 0.0);
    out.reason = get<std::string>(j, "reason", "");
  } else if (type == "prompt_ack") {
    out.type = IncomingType::PromptAck;
    out.prompt = get<std::string>(j, "prompt", "");
    out.success = get<bool>(j, "success", false);
    if (auto it = j.find("error"); it != j.end() && it->is_string()) out.error = it->get<std::string>();
  } else if (type == "set_image_ack") {
    out.type = IncomingType::SetImageAck;
    out.success = get<bool>(j, "success", false);
    if (auto it = j.find("error"); it != j.end() && it->is_string()) out.error = it->get<std::string>();
  } else if (type == "error") {
    out.type = IncomingType::ServerError;
    out.errorMessage = get<std::string>(j, "error", "");
  }
  return out;
}

} // namespace decart::detail
