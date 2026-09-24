// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "decart/logging.h"
#include "decart/models.h"
#include "detail/stream_url.h"

using namespace decart;
using namespace decart::detail;

namespace {

const std::string kBase = "wss://api3.decart.ai";
const std::string kKey = "sk-test";

std::size_t count(const std::string& haystack, const std::string& needle) {
  std::size_t n = 0;
  for (auto pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1)) ++n;
  return n;
}

// Installs a capturing log sink for the duration of a test and restores the
// default sink afterwards.
struct LogCapture {
  std::vector<std::string> warnings;
  LogCapture() {
    setLogHandler([this](LogLevel level, const std::string& message) {
      if (level == LogLevel::Warn) warnings.push_back(message);
    });
  }
  ~LogCapture() { setLogHandler(nullptr); }
};

} // namespace

TEST_CASE("buildStreamUrl without options is unchanged and carries no speed param") {
  const auto model = models::realtime("lucy-2.5");
  const auto url = buildStreamUrl(kBase, model, kKey, std::nullopt, std::nullopt);
  CHECK(url == "wss://api3.decart.ai/v1/stream?api_key=sk-test&model=lucy-2.5");
  CHECK(url.find("speed") == std::string::npos);
  CHECK(url.find("resolution") == std::string::npos);
}

TEST_CASE("buildStreamUrl appends resolution after model") {
  const auto model = models::realtime("lucy-2.5");
  const auto url = buildStreamUrl(kBase, model, kKey, std::string("1080p"), std::nullopt);
  CHECK(url == "wss://api3.decart.ai/v1/stream?api_key=sk-test&model=lucy-2.5&resolution=1080p");
}

TEST_CASE("buildStreamUrl appends speed=fast exactly once, after resolution") {
  const auto model = models::realtime("lucy-2.5");

  SUBCASE("speed only") {
    const auto url = buildStreamUrl(kBase, model, kKey, std::nullopt, Speed::Fast);
    CHECK(url == "wss://api3.decart.ai/v1/stream?api_key=sk-test&model=lucy-2.5&speed=fast");
    CHECK(count(url, "speed=") == 1);
  }

  SUBCASE("speed with resolution") {
    const auto url = buildStreamUrl(kBase, model, kKey, std::string("720p"), Speed::Fast);
    CHECK(url == "wss://api3.decart.ai/v1/stream?api_key=sk-test&model=lucy-2.5&resolution=720p&speed=fast");
    CHECK(count(url, "speed=") == 1);
  }

  SUBCASE("still sent for models without the capability (server ignores it)") {
    const auto restyle = models::realtime("lucy-restyle-2");
    const auto url = buildStreamUrl(kBase, restyle, kKey, std::nullopt, Speed::Fast);
    CHECK(url == "wss://api3.decart.ai/v1/stream?api_key=sk-test&model=lucy-restyle-2&speed=fast");
  }
}

TEST_CASE("buildStreamUrl is deterministic: rebuilding yields the same speed param") {
  // Signaling is single-shot (no reconnect rebuilds the URL), but the builder is
  // pure, so any future re-dial that reuses the same inputs preserves `speed`.
  const auto model = models::realtime("lucy-vton-latest");
  const auto first = buildStreamUrl(kBase, model, kKey, std::nullopt, Speed::Fast);
  const auto second = buildStreamUrl(kBase, model, kKey, std::nullopt, Speed::Fast);
  CHECK(first == second);
  CHECK(count(first, "&speed=fast") == 1);
}

TEST_CASE("supportsSpeed reflects ModelDefinition::supportedSpeeds") {
  CHECK(supportsSpeed(models::realtime("lucy-2.5"), Speed::Fast));
  CHECK(supportsSpeed(models::realtime("lucy-latest"), Speed::Fast));
  CHECK(supportsSpeed(models::realtime("lucy-vton-3.5"), Speed::Fast));
  CHECK(supportsSpeed(models::realtime("lucy-vton-latest"), Speed::Fast));
  CHECK_FALSE(supportsSpeed(models::realtime("lucy-2.1"), Speed::Fast));
  CHECK_FALSE(supportsSpeed(models::realtime("lucy-restyle-2"), Speed::Fast));
  CHECK_FALSE(supportsSpeed(models::realtime("lucy-restyle-latest"), Speed::Fast));
}

TEST_CASE("warnIfSpeedUnsupported logs only for models lacking the capability") {
  LogCapture capture;

  warnIfSpeedUnsupported(models::realtime("lucy-2.5"), Speed::Fast);
  warnIfSpeedUnsupported(models::realtime("lucy-restyle-2"), std::nullopt);
  CHECK(capture.warnings.empty());

  warnIfSpeedUnsupported(models::realtime("lucy-restyle-2"), Speed::Fast);
  REQUIRE(capture.warnings.size() == 1);
  CHECK(capture.warnings[0].find("speed=fast") != std::string::npos);
  CHECK(capture.warnings[0].find("lucy-restyle-2") != std::string::npos);
}
