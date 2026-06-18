// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Decart C++ SDK — umbrella header.
//
// Realtime video transformation and authentication for Decart's models, built
// on the official LiveKit C++ SDK. See https://docs.platform.decart.ai
#pragma once

#include "decart/client.h"
#include "decart/errors.h"
#include "decart/logging.h"
#include "decart/models.h"
#include "decart/tokens.h"
#include "decart/version.h"

// Realtime API — present unless built with DECART_BUILD_REALTIME=OFF.
#ifndef DECART_NO_REALTIME
#include "decart/realtime/realtime.h"
#include "decart/realtime/session.h"
#include "decart/realtime/types.h"
#endif
