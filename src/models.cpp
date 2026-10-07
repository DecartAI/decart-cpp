// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/models.h"

#include <array>

#include "decart/errors.h"

namespace decart {
namespace models {
namespace {

constexpr const char* kStreamPath = "/v1/stream";

struct Entry {
  const char* name;
  int fps;
  int width;
  int height;
  bool canonical;         // false for "latest"/deprecated aliases
  bool fastSpeed = false; // accepts `speed=fast` (see ModelDefinition::supportedSpeeds)
};

// Realtime model registry. Kept in sync with the shared model list across the
// Decart SDKs. All realtime models stream over `/v1/stream`.
constexpr std::array<Entry, 8> kRealtime = {{
    // Canonical
    {"lucy-2.1", 30, 1088, 624, true},
    {"lucy-2.5", 30, 1280, 720, true, /*fastSpeed=*/true},
    {"lucy-vton-3.5", 30, 1280, 720, true, /*fastSpeed=*/true},
    {"lucy-vton-3.6", 30, 1280, 720, true},
    {"lucy-restyle-2", 30, 1280, 704, true},
    // Server-resolved "latest" aliases
    {"lucy-latest", 30, 1088, 624, false, /*fastSpeed=*/true},
    // Resolves server-side to lucy-vton-3.5.
    {"lucy-vton-latest", 30, 1280, 720, false, /*fastSpeed=*/true},
    {"lucy-restyle-latest", 30, 1280, 704, false},
}};

const Entry* find(std::string_view name) {
  for (const auto& e : kRealtime) {
    if (name == e.name) return &e;
  }
  return nullptr;
}

ModelDefinition toDefinition(const Entry& e) {
  ModelDefinition def{e.name, kStreamPath, e.fps, e.width, e.height, /*supportedSpeeds=*/{}};
  if (e.fastSpeed) def.supportedSpeeds.push_back(Speed::Fast);
  return def;
}

} // namespace

ModelDefinition realtime(std::string_view name) {
  if (const Entry* e = find(name)) return toDefinition(*e);
  throw Exception(Error{ErrorCode::ModelNotFound, "Realtime model not found: " + std::string(name)});
}

bool isRealtimeModel(std::string_view name) noexcept { return find(name) != nullptr; }

std::vector<ModelDefinition> listRealtime(bool canonicalOnly) {
  std::vector<ModelDefinition> out;
  for (const auto& e : kRealtime) {
    if (canonicalOnly && !e.canonical) continue;
    out.push_back(toDefinition(e));
  }
  return out;
}

} // namespace models
} // namespace decart
