// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "decart/logging.h"

namespace decart::detail {

/// Emit an SDK log line (respects the configured level and sink).
void log(LogLevel level, const std::string& message);

inline void logInfo(const std::string& m) { log(LogLevel::Info, m); }
inline void logWarn(const std::string& m) { log(LogLevel::Warn, m); }
inline void logError(const std::string& m) { log(LogLevel::Error, m); }
inline void logDebug(const std::string& m) { log(LogLevel::Debug, m); }

} // namespace decart::detail
