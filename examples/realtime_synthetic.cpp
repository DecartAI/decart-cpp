// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Realtime example: publish synthetic color-cycling frames to a Decart model and
// receive the transformed frames. Mirrors the python `realtime_synthetic.py`.
//
//   DECART_API_KEY=sk-... ./realtime_synthetic [model]
//
#include <decart/decart.h>
#include <livekit/livekit.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_running{true};
std::atomic<std::uint64_t> g_remoteFrames{0};

// Fill an RGBA frame with a flat color that cycles every ~25 frames.
livekit::VideoFrame makeFrame(int width, int height, int counter) {
  static const std::uint8_t colors[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
  const auto& c = colors[(counter / 25) % 4];

  livekit::VideoFrame frame = livekit::VideoFrame::create(width, height, livekit::VideoBufferType::RGBA);
  std::uint8_t* data = frame.data();
  for (std::size_t i = 0; i + 4 <= frame.dataSize(); i += 4) {
    data[i] = c[0];
    data[i + 1] = c[1];
    data[i + 2] = c[2];
    data[i + 3] = 255;
  }
  return frame;
}

} // namespace

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
    std::cout << "Model: " << model.name << " (" << model.width << "x" << model.height << " @ " << model.fps
              << "fps)\n";

    auto source = std::make_shared<livekit::VideoSource>(model.width, model.height);

    decart::ConnectOptions options;
    options.model = model;
    options.initialState.prompt = decart::Prompt{"A watercolor painting", true};
    options.onConnectionState = [](decart::ConnectionState state) {
      std::cout << "[state] " << decart::toString(state) << "\n";
    };
    options.onError = [](const decart::Error& error) {
      std::cerr << "[error] " << decart::toString(error.code) << ": " << error.message << "\n";
    };
    options.onRemoteFrame = [](const livekit::VideoFrame& frame, std::int64_t) {
      const auto n = ++g_remoteFrames;
      if (n % 25 == 0) {
        std::cout << "[recv] " << n << " frames (" << frame.width() << "x" << frame.height() << ")\n";
      }
    };

    std::cout << "Connecting...\n";
    auto session = client.realtime().connect(source, options);
    std::cout << "Connected. session=" << session->sessionId().value_or("?") << "\n";

    // Push synthetic frames on a background thread.
    std::thread producer([&] {
      const auto interval = std::chrono::milliseconds(1000 / std::max(1, model.fps));
      int counter = 0;
      while (g_running.load()) {
        source->captureFrame(makeFrame(model.width, model.height, counter++));
        std::this_thread::sleep_for(interval);
      }
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Changing prompt to 'Cyberpunk city'...\n";
    session->setPrompt("Cyberpunk city");

    std::this_thread::sleep_for(std::chrono::seconds(5));

    g_running.store(false);
    producer.join();
    session->disconnect();
    std::cout << "Done. Received " << g_remoteFrames.load() << " transformed frames.\n";
    return 0;
  } catch (const decart::Exception& e) {
    std::cerr << "Decart error [" << decart::toString(e.code()) << "]: " << e.what() << "\n";
    return 1;
  }
}
