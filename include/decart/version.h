// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <string>

/// Major version of the Decart C++ SDK.
#define DECART_SDK_VERSION_MAJOR 0
/// Minor version of the Decart C++ SDK.
#define DECART_SDK_VERSION_MINOR 1
/// Patch version of the Decart C++ SDK.
#define DECART_SDK_VERSION_PATCH 0
/// Full version string of the Decart C++ SDK.
#define DECART_SDK_VERSION "0.1.0"

namespace decart {

/// Returns the SDK version string (e.g. "0.1.0").
inline std::string version() { return DECART_SDK_VERSION; }

} // namespace decart
