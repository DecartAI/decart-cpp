# Copyright 2026 Decart. SPDX-License-Identifier: MIT

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS) # std::getenv in examples
endif()

# Auth/tokens example builds in every configuration.
add_executable(create_token create_token.cpp)
target_link_libraries(create_token PRIVATE decart::decart)
decart_copy_livekit_runtime(create_token)

# Realtime examples require the LiveKit-backed module.
if(DECART_BUILD_REALTIME)
  add_executable(realtime_synthetic realtime_synthetic.cpp)
  target_link_libraries(realtime_synthetic PRIVATE decart::decart)
  decart_copy_livekit_runtime(realtime_synthetic)

  # Connection pre-warming: connect muted (no billing), then unmute to go live.
  add_executable(realtime_warmup realtime_warmup.cpp)
  target_link_libraries(realtime_warmup PRIVATE decart::decart)
  decart_copy_livekit_runtime(realtime_warmup)

  # Streams a video file through a model and saves the result (requires ffmpeg).
  add_executable(realtime_video realtime_video.cpp)
  target_link_libraries(realtime_video PRIVATE decart::decart)
  decart_copy_livekit_runtime(realtime_video)
endif()
