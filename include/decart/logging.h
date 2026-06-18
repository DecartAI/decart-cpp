// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

namespace decart {

/// SDK log severity.
enum class LogLevel { Trace = 0, Debug, Info, Warn, Error, Off };

/// Sink for SDK log messages.
using LogHandler = std::function<void(LogLevel level, const std::string& message)>;

/// Set the minimum level emitted by the SDK (default: Info).
void setLogLevel(LogLevel level);

/// Current minimum log level.
LogLevel logLevel() noexcept;

/// Install a custom log sink. By default messages go to stderr. Pass `nullptr`
/// to restore the default sink.
void setLogHandler(LogHandler handler);

} // namespace decart
