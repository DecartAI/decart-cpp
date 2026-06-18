// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Connection pre-warming: establish an authenticated, idle session up front so
// going live has no startup latency. Billing tracks active generation, so a
// muted warmed connection costs nothing; `unmute()` + capturing frames is what
// starts generation (and billing). An idle WebRTC track still emits keepalive
// frames, so muting — not merely withholding frames — is what keeps it free.
//
//   DECART_API_KEY=sk-... ./realtime_warmup [model]
//
#include <decart/decart.h>
#include <livekit/livekit.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char** argv) {
  const char* apiKey = std::getenv("DECART_API_KEY");
  if (apiKey == nullptr) {
    std::cerr << "Set DECART_API_KEY to run this example.\n";
    return 1;
  }
  const std::string modelName = argc > 1 ? argv[1] : "lucy-restyle-2";

  try {
    decart::Client client; // reads DECART_API_KEY
    const decart::ModelDefinition model = decart::models::realtime(modelName);
    auto source = std::make_shared<livekit::VideoSource>(model.width, model.height);

    decart::ConnectOptions options;
    options.model = model;
    options.initialState.prompt = decart::Prompt{"A watercolor painting", true};
    options.startMuted = true; // warm the connection without transmitting (no billing)
    options.onConnectionState = [](decart::ConnectionState state) {
      // Stays "connected" while warmed; flips to "generating" once frames flow.
      std::cout << "[state] " << decart::toString(state) << "\n";
    };

    // 1. Warmup: connects, authenticates, applies the prompt — transmits nothing.
    std::cout << "Warming up...\n";
    auto session = client.realtime().connect(source, options);
    std::cout << "Warmed and idle (muted) — no billing. Holding...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));
    // (To cancel before going live: session->disconnect() — no charge.)

    // 2. Go live: unmute, then push ~5s of frames. Billing starts here.
    std::cout << "Go live!\n";
    session->unmute();
    const auto interval = std::chrono::milliseconds(1000 / std::max(1, model.fps));
    for (int i = 0; i < model.fps * 5; ++i) {
      auto frame = livekit::VideoFrame::create(model.width, model.height, livekit::VideoBufferType::RGBA);
      std::fill(frame.data(), frame.data() + frame.dataSize(), std::uint8_t{128}); // solid gray
      source->captureFrame(frame);
      std::this_thread::sleep_for(interval);
    }

    session->disconnect();
    return 0;
  } catch (const decart::Exception& e) {
    std::cerr << "Decart error [" << decart::toString(e.code()) << "]: " << e.what() << "\n";
    return 1;
  }
}
