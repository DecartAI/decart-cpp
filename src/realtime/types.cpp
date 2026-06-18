// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/realtime/types.h"

namespace decart {

const char* toString(ConnectionState state) noexcept {
  switch (state) {
    case ConnectionState::Connecting:
      return "connecting";
    case ConnectionState::Connected:
      return "connected";
    case ConnectionState::Generating:
      return "generating";
    case ConnectionState::Disconnected:
      return "disconnected";
    case ConnectionState::Reconnecting:
      return "reconnecting";
  }
  return "unknown";
}

} // namespace decart
