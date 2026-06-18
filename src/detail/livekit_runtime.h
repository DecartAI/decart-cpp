// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

namespace decart::detail {

/// Initialize the LiveKit runtime exactly once (idempotent). Must run before any
/// `livekit::` object is constructed — including the `VideoSource` the caller
/// builds before `connect()` — so the `Client` constructor calls it eagerly.
void ensureLiveKitInit();

} // namespace decart::detail
