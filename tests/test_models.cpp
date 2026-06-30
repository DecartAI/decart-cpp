// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include <doctest/doctest.h>

#include "decart/errors.h"
#include "decart/models.h"

using namespace decart;

TEST_CASE("realtime() resolves canonical models with correct geometry") {
  auto m = models::realtime("lucy-restyle-2");
  CHECK(m.name == "lucy-restyle-2");
  CHECK(m.urlPath == "/v1/stream");
  CHECK(m.fps == 30);
  CHECK(m.width == 1280);
  CHECK(m.height == 704);

  auto vton = models::realtime("lucy-vton-3");
  CHECK(vton.width == 1088);
  CHECK(vton.height == 624);
}

TEST_CASE("realtime() accepts latest and deprecated aliases") {
  CHECK(models::realtime("lucy-latest").urlPath == "/v1/stream");
  CHECK(models::realtime("lucy-vton-latest").name == "lucy-vton-latest");
  CHECK(models::realtime("mirage_v2").width == 1280); // deprecated -> restyle geometry
  CHECK(models::isRealtimeModel("lucy-2.1-vton-2"));
}

TEST_CASE("realtime() throws ModelNotFound for unknown names") {
  CHECK_FALSE(models::isRealtimeModel("not-a-model"));
  CHECK_THROWS_AS(models::realtime("not-a-model"), Exception);
  try {
    models::realtime("nope");
  } catch (const Exception& e) {
    // Compare via integer form: doctest stringifies enums through our ADL
    // toString() (which returns const char*) and cannot concatenate that.
    CHECK(static_cast<int>(e.code()) == static_cast<int>(ErrorCode::ModelNotFound));
  }
}

TEST_CASE("listRealtime() honors canonicalOnly") {
  auto all = models::listRealtime(/*canonicalOnly=*/false);
  auto canonical = models::listRealtime(/*canonicalOnly=*/true);
  CHECK(canonical.size() == 4);
  CHECK(all.size() > canonical.size());
}
