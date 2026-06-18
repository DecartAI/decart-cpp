// Copyright 2026 Decart. SPDX-License-Identifier: MIT
#include "decart/errors.h"

namespace decart {

const char* toString(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::InvalidApiKey:
      return "INVALID_API_KEY";
    case ErrorCode::InvalidBaseUrl:
      return "INVALID_BASE_URL";
    case ErrorCode::InvalidInput:
      return "INVALID_INPUT";
    case ErrorCode::ModelNotFound:
      return "MODEL_NOT_FOUND";
    case ErrorCode::TokenCreateError:
      return "TOKEN_CREATE_ERROR";
    case ErrorCode::WebsocketError:
      return "WEBRTC_WEBSOCKET_ERROR";
    case ErrorCode::TimeoutError:
      return "WEBRTC_TIMEOUT_ERROR";
    case ErrorCode::ServerError:
      return "WEBRTC_SERVER_ERROR";
    case ErrorCode::SignalingError:
      return "WEBRTC_SIGNALING_ERROR";
    case ErrorCode::MediaError:
      return "WEBRTC_MEDIA_ERROR";
    case ErrorCode::NotConnected:
      return "NOT_CONNECTED";
    case ErrorCode::HttpError:
      return "HTTP_ERROR";
  }
  return "UNKNOWN";
}

} // namespace decart
