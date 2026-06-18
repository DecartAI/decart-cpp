// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/realtime/realtime.h"

#include "detail/client_config.h"
#include "realtime/session_internal.h"

namespace decart {

RealtimeClient::RealtimeClient(std::shared_ptr<detail::ClientConfig> config) : config_(std::move(config)) {}

std::unique_ptr<RealtimeSession> RealtimeClient::connect(std::shared_ptr<livekit::VideoSource> source,
                                                         ConnectOptions options) {
  return detail::connectRealtime(config_, std::move(source), std::move(options));
}

} // namespace decart
