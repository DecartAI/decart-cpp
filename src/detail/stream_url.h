// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <optional>
#include <string>

#include "decart/models.h"
#include "decart/realtime/types.h"
#include "detail/log.h"
#include "detail/url.h"

namespace decart::detail {

/// Build the realtime signaling URL for a session: `<base><model.urlPath>` plus
/// the `api_key`, `model`, and optional `resolution` / `speed` query parameters,
/// in that order. Optional parameters are omitted entirely when unset. The
/// `user_agent` parameter is appended later by SignalingChannel. Pure, so it can
/// be unit-tested without LiveKit.
inline std::string buildStreamUrl(const std::string& base, const ModelDefinition& model,
                                  const std::string& apiKey, const std::optional<std::string>& resolution,
                                  std::optional<Speed> speed) {
  std::string url = base + model.urlPath;
  url = appendQuery(url, "api_key", apiKey);
  url = appendQuery(url, "model", model.name);
  if (resolution.has_value()) url = appendQuery(url, "resolution", *resolution);
  if (speed.has_value()) url = appendQuery(url, "speed", toString(*speed));
  return url;
}

/// True when `model` advertises `speed` in `ModelDefinition::supportedSpeeds`.
inline bool supportsSpeed(const ModelDefinition& model, Speed speed) noexcept {
  return std::find(model.supportedSpeeds.begin(), model.supportedSpeeds.end(), speed) !=
         model.supportedSpeeds.end();
}

/// Log a warning when `speed` is requested for a model that does not advertise
/// it. Not an error: the parameter is still sent and the server ignores it,
/// serving (and billing) the standard tier.
inline void warnIfSpeedUnsupported(const ModelDefinition& model, std::optional<Speed> speed) {
  if (!speed.has_value() || supportsSpeed(model, *speed)) return;
  logWarn("speed=" + std::string(toString(*speed)) + " is not supported by model '" + model.name +
          "'; the server will ignore it and serve the standard tier");
}

} // namespace decart::detail
