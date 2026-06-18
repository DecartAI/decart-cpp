// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Pure (de)serialization of the Decart realtime signaling protocol. No
// transport, so this is unit-testable on its own. See the wire protocol in the
// other SDKs (e.g. python `realtime/messages.py`, js `realtime/types.ts`).
#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace decart::detail {

// ---- Outgoing message builders ------------------------------------------------

/// `{"type":"prompt","prompt":<text>,"enhance_prompt":<enhance>}`
nlohmann::json makePromptMsg(const std::string& text, bool enhance);

/// `{"type":"set_image","image_data":<base64|null>[,"prompt":...][,"enhance_prompt":...]}`
/// A nullopt `base64` serializes `image_data: null` (clears the image).
nlohmann::json makeSetImageDataMsg(const std::optional<std::string>& base64,
                                   const std::optional<std::string>& prompt,
                                   const std::optional<bool>& enhance);

/// `{"type":"set_image","image_ref":<ref>[,"prompt":...][,"enhance_prompt":...]}`
nlohmann::json makeSetImageRefMsg(const std::string& ref, const std::optional<std::string>& prompt,
                                  const std::optional<bool>& enhance);

/// `{"type":"livekit_join","initial_state":<msg|null>}`
nlohmann::json makeJoinMsg(const std::optional<nlohmann::json>& initialState);

// ---- Incoming message parsing -------------------------------------------------

enum class IncomingType {
  Unknown,
  LiveKitRoomInfo,
  QueuePosition,
  GenerationStarted,
  GenerationTick,
  GenerationEnded,
  PromptAck,
  SetImageAck,
  ServerError,
};

/// A parsed incoming signaling message. Only the fields relevant to `type` are
/// populated.
struct IncomingMessage {
  IncomingType type = IncomingType::Unknown;

  // LiveKitRoomInfo
  std::string livekitUrl;
  std::string token;
  std::string roomName;
  std::string sessionId;

  // QueuePosition
  int position = 0;
  int queueSize = 0;

  // GenerationTick / GenerationEnded
  double seconds = 0.0;
  std::string reason;

  // PromptAck / SetImageAck
  std::string prompt; // echoed prompt (PromptAck)
  bool success = false;
  std::optional<std::string> error;

  // ServerError
  std::string errorMessage;
};

/// Parse one incoming JSON text frame. Returns `type == Unknown` for malformed
/// input or unrecognized message types (callers should ignore those).
IncomingMessage parseIncoming(const std::string& text);

} // namespace decart::detail
