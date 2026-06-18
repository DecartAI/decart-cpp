# Copyright 2026 Decart. SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20)

project(decart
  VERSION 0.2.0
  DESCRIPTION "Decart C++ SDK — realtime video transformation and auth"
  HOMEPAGE_URL "https://github.com/DecartAI/decart-cpp"
  LANGUAGES CXX)

# --- Standard / output ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Treat this as the top-level project only when not added via add_subdirectory.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(DECART_IS_TOP_LEVEL ON)
else()
  set(DECART_IS_TOP_LEVEL OFF)
endif()

# --- Options -------------------------------------------------------------------
option(DECART_BUILD_EXAMPLES "Build example applications" ${DECART_IS_TOP_LEVEL})
option(DECART_BUILD_TESTS "Build unit tests" ${DECART_IS_TOP_LEVEL})
# When OFF, builds an auth/tokens-only library with no LiveKit dependency (no Rust
# toolchain, no FFI download) — useful for server-side token minting. The realtime
# API (Client::realtime()) is then compiled out.
option(DECART_BUILD_REALTIME "Build the realtime (LiveKit-backed) module" ON)
set(DECART_LIVEKIT_VERSION "latest" CACHE STRING "LiveKit C++ SDK release to download (e.g. 0.2.0 or latest)")
set(DECART_LIVEKIT_LOCAL_DIR "" CACHE PATH "Path to a local LiveKit SDK install prefix (skips download)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

# Tracks whether any dependency was pulled via FetchContent (rather than found as
# an installed package). A relocatable install/export is only valid against found
# packages, so this gates the install rules below.
set(_decart_fetched_deps OFF)

# --- Dependencies: nlohmann_json (vcpkg/system, else fetch) --------------------
find_package(nlohmann_json CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
  message(STATUS "decart: fetching nlohmann_json")
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
  FetchContent_MakeAvailable(nlohmann_json)
  set(_decart_fetched_deps ON)
endif()

# --- Dependencies: IXWebSocket (WebSocket signaling + HTTP) ---------------------
find_package(ixwebsocket CONFIG QUIET)
if(NOT TARGET ixwebsocket::ixwebsocket AND NOT TARGET ixwebsocket)
  message(STATUS "decart: fetching IXWebSocket")
  set(USE_TLS ON CACHE BOOL "" FORCE)
  FetchContent_Declare(ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.6)
  FetchContent_MakeAvailable(ixwebsocket)
  set(_decart_fetched_deps ON)
endif()
if(TARGET ixwebsocket::ixwebsocket)
  set(DECART_IXWEBSOCKET ixwebsocket::ixwebsocket)
else()
  set(DECART_IXWEBSOCKET ixwebsocket)
endif()

# --- Dependencies: LiveKit C++ SDK (realtime media transport) -------------------
if(DECART_BUILD_REALTIME)
  if(DECART_LIVEKIT_LOCAL_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${DECART_LIVEKIT_LOCAL_DIR}")
  else()
    include(LiveKitSDK)
    livekit_sdk_setup(
      VERSION "${DECART_LIVEKIT_VERSION}"
      SDK_DIR "${CMAKE_BINARY_DIR}/_deps/livekit-sdk"
      GITHUB_TOKEN "$ENV{GITHUB_TOKEN}")
  endif()
  find_package(LiveKit CONFIG REQUIRED)
  if(TARGET LiveKit::livekit)
    set(DECART_LIVEKIT LiveKit::livekit)
  elseif(TARGET livekit)
    set(DECART_LIVEKIT livekit)
  else()
    message(FATAL_ERROR "decart: LiveKit core target not found (LiveKit::livekit / livekit)")
  endif()
endif()

# --- Library -------------------------------------------------------------------
# Always built: client, auth/tokens, models, and HTTP/JSON plumbing.
set(DECART_SOURCES
  src/client.cpp
  src/errors.cpp
  src/logging.cpp
  src/models.cpp
  src/tokens.cpp
  src/detail/base64.cpp
  src/detail/http_client.cpp
  src/detail/user_agent.cpp
  src/realtime/types.cpp)

# Realtime (LiveKit-backed) sources, only when enabled.
if(DECART_BUILD_REALTIME)
  list(APPEND DECART_SOURCES
    src/detail/signaling_channel.cpp
    src/realtime/messages.cpp
    src/realtime/realtime.cpp
    src/realtime/session.cpp)
endif()

add_library(decart ${DECART_SOURCES})
add_library(decart::decart ALIAS decart)

# Single source of truth for the version: the project() VERSION above. Generate
# <decart/version.h> from it so headers can never drift from the release tag.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/decart/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/decart/version.h"
  @ONLY)

target_include_directories(decart
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(decart PUBLIC cxx_std_17)
# nlohmann_json is header-only and used only in .cpp files: needed to build, never
# part of decart's link/usage interface.
target_link_libraries(decart PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
# IXWebSocket powers both the tokens HTTP client and realtime signaling.
target_link_libraries(decart PRIVATE ${DECART_IXWEBSOCKET})

if(WIN32)
  # IXWebSocket's TLS backend (mbedtls) needs Windows CNG for entropy
  # (BCryptGenRandom). PUBLIC so it resolves at the final link of consumers.
  target_link_libraries(decart PUBLIC bcrypt)
endif()

if(DECART_BUILD_REALTIME)
  # LiveKit is PUBLIC: consumers create livekit::VideoSource / receive livekit::VideoFrame.
  target_link_libraries(decart PUBLIC ${DECART_LIVEKIT})
else()
  # Compiled out of the public headers and consumers (propagated via INTERFACE).
  target_compile_definitions(decart PUBLIC DECART_NO_REALTIME)
endif()

if(MSVC)
  target_compile_options(decart PRIVATE /W4)
  target_compile_definitions(decart PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(decart PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy the LiveKit FFI runtime library next to a target's executable.
function(decart_copy_livekit_runtime target)
  if(NOT DEFINED LiveKit_DIR)
    return()
  endif()
  get_filename_component(_cmake_dir "${LiveKit_DIR}" DIRECTORY)  # <root>/lib/cmake
  get_filename_component(_lib_dir "${_cmake_dir}" DIRECTORY)     # <root>/lib
  get_filename_component(_root "${_lib_dir}" DIRECTORY)          # <root>
  if(WIN32)
    foreach(_dll livekit.dll livekit_ffi.dll)
      if(EXISTS "${_root}/bin/${_dll}")
        add_custom_command(TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${_root}/bin/${_dll}" "$<TARGET_FILE_DIR:${target}>")
      endif()
    endforeach()
  else()
    file(GLOB _ffi_libs "${_root}/lib/*livekit_ffi*")
    foreach(_ffi ${_ffi_libs})
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_ffi}" "$<TARGET_FILE_DIR:${target}>")
    endforeach()
  endif()
endfunction()

# --- Subdirectories ------------------------------------------------------------
if(DECART_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(DECART_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --- Install / export ----------------------------------------------------------
# A relocatable package can only be exported when decart's dependencies are
# themselves installed packages (found via find_package), not FetchContent build
# targets. Default the install rules accordingly; users can override.
if(_decart_fetched_deps)
  set(_decart_install_default OFF)
else()
  set(_decart_install_default ${DECART_IS_TOP_LEVEL})
endif()
option(DECART_INSTALL "Generate install/export rules for decart" ${_decart_install_default})

if(DECART_INSTALL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS decart
  EXPORT decartTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Public headers, excluding the .in template (the generated header is installed below).
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN "*.in" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/decart/version.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/decart)

install(EXPORT decartTargets
  FILE decartTargets.cmake
  NAMESPACE decart::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/decart)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/decart-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/decart-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/decart)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/decart-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/decart-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/decart-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/decart)
endif() # DECART_INSTALL
