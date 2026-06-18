// Copyright 2026 Decart. SPDX-License-Identifier: MIT
//
// Stream a video FILE through a Decart realtime model and save the transformed
// result. Uses ffmpeg (must be on PATH) to decode the input to raw RGBA frames
// and to encode the returned frames back into an .mp4.
//
//   DECART_API_KEY=... ./realtime_video <input.mp4> <output.mp4> ["prompt"] [model]
//
// Set DECART_DEBUG=1 for verbose SDK logging.
#include <decart/decart.h>
#include <livekit/livekit.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// MSVC spells the POSIX pipe functions with a leading underscore.
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace {

FILE* openDecodePipe(const std::string& input, int w, int h, int fps) {
  // Scale to the model resolution, preserving aspect ratio with black padding.
  std::string cmd = "ffmpeg -hide_banner -loglevel error -i \"" + input + "\"" +
                    " -vf \"scale=" + std::to_string(w) + ":" + std::to_string(h) +
                    ":force_original_aspect_ratio=decrease,pad=" + std::to_string(w) + ":" +
                    std::to_string(h) + ":(ow-iw)/2:(oh-ih)/2,fps=" + std::to_string(fps) +
                    "\" -f rawvideo -pix_fmt rgba -";
  return popen(cmd.c_str(), "r");
}

FILE* openEncodePipe(const std::string& output, int w, int h, int fps) {
  std::string cmd = "ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt rgba -s " +
                    std::to_string(w) + "x" + std::to_string(h) + " -r " + std::to_string(fps) +
                    " -i - -c:v libx264 -pix_fmt yuv420p \"" + output + "\"";
  return popen(cmd.c_str(), "w");
}

} // namespace

int main(int argc, char** argv) {
  if (std::getenv("DECART_API_KEY") == nullptr) {
    std::cerr << "Set DECART_API_KEY to run this example.\n";
    return 1;
  }
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <input.mp4> <output.mp4> [\"prompt\"] [model]\n";
    return 1;
  }
  const std::string input = argv[1];
  const std::string output = argv[2];
  const std::string prompt = argc > 3 ? argv[3] : "put them all in space";
  const std::string modelName = argc > 4 ? argv[4] : "lucy-2.1";

  if (std::getenv("DECART_DEBUG") != nullptr) decart::setLogLevel(decart::LogLevel::Debug);

  try {
    decart::Client client;
    const decart::ModelDefinition model = decart::models::realtime(modelName);
    const int w = model.width, h = model.height, fps = model.fps;
    std::cout << "Model " << model.name << " " << w << "x" << h << "@" << fps << "  prompt=\"" << prompt
              << "\"\n";

    auto source = std::make_shared<livekit::VideoSource>(w, h);

    // Output encoder is opened lazily once we know the returned frame size.
    std::mutex encMutex;
    FILE* encodePipe = nullptr;
    int outW = 0, outH = 0;
    std::atomic<std::uint64_t> recvFrames{0};

    decart::ConnectOptions options;
    options.model = model;
    options.initialState.prompt = decart::Prompt{prompt, /*enhance=*/true};
    options.onConnectionState = [](decart::ConnectionState s) {
      std::cout << "[state] " << decart::toString(s) << "\n";
    };
    options.onError = [](const decart::Error& e) {
      std::cerr << "[error] " << decart::toString(e.code) << ": " << e.message << "\n";
    };
    options.onRemoteFrame = [&](const livekit::VideoFrame& frame, std::int64_t) {
      std::lock_guard<std::mutex> lk(encMutex);
      if (encodePipe == nullptr) {
        outW = frame.width();
        outH = frame.height();
        encodePipe = openEncodePipe(output, outW, outH, fps);
        std::cout << "[recv] first frame " << outW << "x" << outH << " -> encoding to " << output << "\n";
      }
      if (encodePipe && frame.width() == outW && frame.height() == outH) {
        std::fwrite(frame.data(), 1, frame.dataSize(), encodePipe);
        if (++recvFrames % 30 == 0) std::cout << "[recv] " << recvFrames << " frames\n";
      }
    };

    std::cout << "Connecting...\n";
    auto session = client.realtime().connect(source, options);
    std::cout << "Connected. session=" << session->sessionId().value_or("?") << "\n";

    FILE* decodePipe = openDecodePipe(input, w, h, fps);
    if (decodePipe == nullptr) {
      std::cerr << "Failed to start ffmpeg decode for " << input << "\n";
      return 1;
    }

    const std::size_t frameBytes = static_cast<std::size_t>(w) * h * 4;
    std::vector<std::uint8_t> buf(frameBytes);
    const auto interval = std::chrono::milliseconds(1000 / std::max(1, fps));
    std::uint64_t fed = 0;
    auto next = std::chrono::steady_clock::now();
    while (std::fread(buf.data(), 1, frameBytes, decodePipe) == frameBytes) {
      source->captureFrame(livekit::VideoFrame(w, h, livekit::VideoBufferType::RGBA, buf));
      if (++fed % 30 == 0) std::cout << "[send] " << fed << " frames\n";
      next += interval;
      std::this_thread::sleep_until(next);
    }
    pclose(decodePipe);
    std::cout << "Fed " << fed << " frames; draining output...\n";

    // Let trailing transformed frames arrive before tearing down.
    std::this_thread::sleep_for(std::chrono::seconds(3));
    session->disconnect();

    {
      std::lock_guard<std::mutex> lk(encMutex);
      if (encodePipe) pclose(encodePipe);
    }
    std::cout << "Done. Sent " << fed << ", received " << recvFrames.load() << " frames -> " << output
              << "\n";
    return recvFrames.load() > 0 ? 0 : 2;
  } catch (const decart::Exception& e) {
    std::cerr << "Decart error [" << decart::toString(e.code()) << "]: " << e.what() << "\n";
    return 1;
  }
}
