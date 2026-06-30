// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace decart {

/// A realtime model definition: everything the SDK needs to open a session and
/// size the input video. Mirrors the `models.realtime(...)` registry shared by
/// the other Decart SDKs.
struct ModelDefinition {
  /// Canonical model name sent to the server (e.g. "lucy-restyle-2").
  std::string name;
  /// Signaling URL path for the model (all realtime models use "/v1/stream").
  std::string urlPath;
  /// Recommended capture frame rate.
  int fps = 30;
  /// Recommended capture width in pixels.
  int width = 0;
  /// Recommended capture height in pixels.
  int height = 0;
};

namespace models {

/// Resolve a realtime model by name.
///
/// Accepts canonical names ("lucy-2.1", "lucy-vton-2",
/// "lucy-vton-3", "lucy-restyle-2"), server-resolved aliases ("lucy-latest",
/// "lucy-vton-latest", "lucy-restyle-latest"), and deprecated names (which emit
/// no error but resolve to the same stream endpoint).
///
/// @throws Exception(ErrorCode::ModelNotFound) if the name is unknown.
ModelDefinition realtime(std::string_view name);

/// Returns true if `name` is a known realtime model name.
bool isRealtimeModel(std::string_view name) noexcept;

/// List all realtime model definitions known to the SDK.
/// @param canonicalOnly when true, excludes deprecated and "latest" aliases.
std::vector<ModelDefinition> listRealtime(bool canonicalOnly = false);

} // namespace models
} // namespace decart
