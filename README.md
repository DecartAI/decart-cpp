# Decart C++ SDK

A C++ SDK for [Decart](https://decart.ai)'s realtime models. It speaks the same
realtime + auth protocol as the [JavaScript](https://github.com/DecartAI/sdk),
[Python](https://github.com/DecartAI/decart-python), iOS, and Android SDKs, and
is built on the official [LiveKit C++ SDK](https://github.com/livekit/client-sdk-cpp)
for media transport.

> **Status:** v0.1 — focused on realtime video transformation and authentication.

## Features

- **Realtime** — publish a video stream, get back the model-transformed stream,
  and steer it live with prompts and reference images.
- **Auth** — mint short-lived client tokens from a server-side API key.
- Native, header-light public API; LiveKit types only where you actually touch media.

## Requirements

- **C++17** and **CMake ≥ 3.20**
- Platforms (from the LiveKit C++ SDK): **Linux** (x64/arm64), **macOS** 12.3+
  (Apple Silicon & Intel), **Windows** (x64)
- Dependencies (resolved automatically — see below):
  [LiveKit C++ SDK](https://github.com/livekit/client-sdk-cpp),
  [IXWebSocket](https://github.com/machinezone/IXWebSocket),
  [nlohmann/json](https://github.com/nlohmann/json)

## Installation

### CMake `FetchContent` (simplest)

```cmake
include(FetchContent)
FetchContent_Declare(decart
  GIT_REPOSITORY https://github.com/DecartAI/decart-cpp.git
  GIT_TAG v0.1.0)
FetchContent_MakeAvailable(decart)

target_link_libraries(your_app PRIVATE decart::decart)
```

The first configure downloads a prebuilt LiveKit release and fetches IXWebSocket /
nlohmann/json. Set `DECART_LIVEKIT_VERSION` to pin a LiveKit release, or
`DECART_LIVEKIT_LOCAL_DIR` to point at a local install. The LiveKit FFI runtime
library is copied next to your executable automatically via
`decart_copy_livekit_runtime(your_app)`.

### vcpkg

`nlohmann-json` and `ixwebsocket` are declared in `vcpkg.json`. Configure with the
vcpkg toolchain (LiveKit is still fetched as a prebuilt release):

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### `add_subdirectory`

```cmake
add_subdirectory(third_party/decart-cpp)
target_link_libraries(your_app PRIVATE decart::decart)
```

## Quick start

### Realtime video transformation

You drive a `livekit::VideoSource` (push your camera/render frames into it); the
SDK publishes it and hands you the transformed frames.

```cpp
#include <decart/decart.h>
#include <livekit/livekit.h>

decart::Client client;                                   // reads DECART_API_KEY
auto model = decart::models::realtime("lucy-restyle-2");
auto source = std::make_shared<livekit::VideoSource>(model.width, model.height);

decart::ConnectOptions options;
options.model = model;
options.initialState.prompt = decart::Prompt{"A watercolor painting", /*enhance=*/true};
options.onConnectionState = [](decart::ConnectionState s) {
  std::printf("state: %s\n", decart::toString(s));
};
options.onRemoteFrame = [](const livekit::VideoFrame& frame, std::int64_t /*ts_us*/) {
  // Render `frame` (RGBA). Runs on a reader thread — keep it quick.
};

auto session = client.realtime().connect(source, options);

// Push frames into `source` from your capture/render loop:
//   source->captureFrame(myFrame);

// Steer the model live:
session->setPrompt("Cyberpunk city");

session->disconnect();
```

See [`examples/realtime_synthetic.cpp`](examples/realtime_synthetic.cpp) for a
complete, runnable program that publishes synthetic frames, and
[`examples/realtime_video.cpp`](examples/realtime_video.cpp) to stream a real
video file through a model and save the transformed result (uses `ffmpeg` to
decode/encode):

```bash
DECART_API_KEY=... ./realtime_video input.mp4 output.mp4 "put them all in space" lucy-2.1
```

### Authentication (client tokens)

Create a short-lived token server-side to hand to an untrusted client:

```cpp
decart::Client client;                              // server-side API key
decart::CreateTokenOptions opts;
opts.expiresIn = 300;                               // seconds
opts.allowedModels = {"lucy-restyle-2"};
auto token = client.tokens().create(opts);
// token.apiKey -> "ek_...", token.expiresAt -> ISO-8601
```

## Configuration

```cpp
decart::ClientOptions options;
options.apiKey = "sk-...";                  // or set DECART_API_KEY
options.baseUrl = "https://api.decart.ai";  // HTTP API (tokens)
options.realtimeBaseUrl = "wss://api3.decart.ai"; // signaling
options.integration = "my-app";             // included in the User-Agent
decart::Client client(options);
```

Errors surface as `decart::Exception` (synchronous failures) carrying a structured
`decart::Error{code, message}`; asynchronous session errors are delivered to
`ConnectOptions::onError`.

## How it works

A realtime session is two channels: a **WebSocket signaling** connection to Decart
that performs the room handshake and carries prompt/image control messages, and a
**LiveKit (WebRTC)** room that carries the media. The SDK opens the socket, joins,
publishes your `VideoSource`, subscribes to the inference server's output track, and
pumps decoded frames to your `onRemoteFrame` callback.

## Building from source

```bash
cmake --preset release          # configure (downloads deps)
cmake --build --preset release  # build library, examples, tests
ctest --preset release          # run unit tests
```

Useful options: `DECART_BUILD_EXAMPLES`, `DECART_BUILD_TESTS`,
`DECART_LIVEKIT_VERSION` (pin a LiveKit release), `DECART_LIVEKIT_LOCAL_DIR`
(use a local LiveKit install instead of downloading), and
`DECART_BUILD_REALTIME=OFF` for an **auth/tokens-only build with no LiveKit
dependency** (no Rust toolchain, no FFI download) — `Client::realtime()` is then
compiled out, leaving `Client::tokens()`.

## v0.1 scope & notes

- **Input frames** are provided by you via `livekit::VideoSource` (full control
  over your capture pipeline). Audio is not published in v0.1.
- **Reference images** accept a local file path, a `data:` URL, or raw base64.
- **Reconnection**: LiveKit auto-recovers the media transport; the signaling
  socket is single-shot in v0.1 (a drop after connect is surfaced via `onError`).
- Not yet included (planned): `checkConnectivity` preflight, audio, queue/image
  generation APIs, proxy mode.

## License

MIT — see [LICENSE](LICENSE).
