# Decart C++ SDK — Coding Agent Guidelines

## Build / test commands

- `cmake --preset release` — configure (downloads LiveKit, fetches IXWebSocket/json)
- `cmake --build --preset release` — build library, examples, tests
- `ctest --preset release` — run unit tests (or run `build/release/tests/decart_tests`)
- `./scripts/format.sh` / `clang-format -i` — format (`.clang-format`, 110 cols, Google base)
- Auth/tokens-only build (no LiveKit): `cmake -B build -S . -DDECART_BUILD_REALTIME=OFF`
  (compiles out `Client::realtime()` via the `DECART_NO_REALTIME` interface define)

## Architecture

A realtime session = **WebSocket signaling** (control) + **LiveKit/WebRTC** (media).

- `include/decart/` — public API. `client.h` (Client/ClientOptions), `models.h`
  (model registry), `tokens.h` (auth), `errors.h`, `logging.h`,
  `realtime/{realtime,session,types}.h`.
- `src/` — implementation.
  - `client.cpp`, `models.cpp`, `tokens.cpp`, `logging.cpp`, `errors.cpp`
  - `detail/` — internal helpers: `http_client` (IXWebSocket HTTP), `signaling_channel`
    (WebSocket + ack/event dispatch), `base64`, `url`, `user_agent`, `client_config`, `log`.
  - `realtime/` — `messages` (pure JSON codec, unit-tested), `realtime` (RealtimeClient
    forwarders), `session` (the orchestration + LiveKit `RoomDelegate` + frame pump).
- `tests/` — doctest unit tests for the dependency-light core (models, message codec,
  base64, enums). Keep new pure logic testable here.

## Conventions

- **C++17**. PascalCase types, camelCase methods, `decart::` namespace, internal
  helpers under `decart::detail`. This matches the LiveKit C++ SDK ergonomics, since
  callers mix `decart::` and `livekit::` types in the same code.
- Public headers stay light: forward-declare `livekit::VideoSource`/`VideoFrame`
  rather than including LiveKit; never expose `nlohmann::json` in the public API.
- Errors: throw `decart::Exception{Error{code, message}}` for synchronous failures;
  deliver async session errors via the `onError` callback. Codes mirror the other SDKs.
- The signaling protocol's wire shapes live in `src/realtime/messages.*` — keep them
  byte-compatible with the other SDKs (`type`, `prompt`, `enhance_prompt`,
  `image_data`/`image_ref`, `livekit_join`, `livekit_room_info`, acks, etc.).

## Gotchas

- The released LiveKit `RoomOptions` is narrower than `main` — validate API usage
  against the **release** headers (downloaded to `build/*/_deps/livekit-sdk/...`),
  not the upstream `main` branch.
- `VideoCodec` is forward-declared only in LiveKit's public headers, so the publish
  codec cannot be set from consumer code — leave it at the default.
- Never call `room.disconnect()` (or `session->disconnect()`) from inside a
  delegate/session callback — it deadlocks the LiveKit event thread.
- The LiveKit FFI shared library must sit next to the executable; use
  `decart_copy_livekit_runtime(target)` for any binary that links `decart`.
