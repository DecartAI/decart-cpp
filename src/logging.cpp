// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/logging.h"

#include <atomic>
#include <iostream>
#include <mutex>

#include "detail/log.h"

namespace decart {
namespace {

std::mutex g_mutex;
LogLevel g_level = LogLevel::Info;
LogHandler g_handler; // empty => default stderr sink

const char* levelName(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return "TRACE";
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warn:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
    case LogLevel::Off:
      return "OFF";
  }
  return "?";
}

} // namespace

void setLogLevel(LogLevel level) {
  std::lock_guard<std::mutex> lk(g_mutex);
  g_level = level;
}

LogLevel logLevel() noexcept {
  std::lock_guard<std::mutex> lk(g_mutex);
  return g_level;
}

void setLogHandler(LogHandler handler) {
  std::lock_guard<std::mutex> lk(g_mutex);
  g_handler = std::move(handler);
}

namespace detail {

void log(LogLevel level, const std::string& message) {
  LogHandler handler;
  {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (static_cast<int>(level) < static_cast<int>(g_level) || g_level == LogLevel::Off) return;
    handler = g_handler;
  }
  if (handler) {
    handler(level, message);
  } else {
    std::cerr << "[decart] " << levelName(level) << " " << message << "\n";
  }
}

} // namespace detail
} // namespace decart
