// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#pragma once

#include <memory>

#include "decart/realtime/realtime.h"
#include "detail/client_config.h"

namespace livekit {
class VideoSource;
}

namespace decart::detail {

/// Orchestrates a publishing realtime session: signaling handshake + LiveKit
/// room connect + publish + remote-frame pump. Implemented in session.cpp.
std::unique_ptr<RealtimeSession> connectRealtime(std::shared_ptr<ClientConfig> config,
                                                 std::shared_ptr<livekit::VideoSource> source,
                                                 ConnectOptions options);

} // namespace decart::detail
