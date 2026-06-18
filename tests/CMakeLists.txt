# Copyright 2026 Decart. SPDX-License-Identifier: MIT

# doctest: from vcpkg/system, else fetched.
find_package(doctest CONFIG QUIET)
if(NOT TARGET doctest::doctest)
  message(STATUS "decart: fetching doctest")
  # doctest's CMakeLists predates CMake 4's 3.5 policy floor; allow it.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11)
  FetchContent_MakeAvailable(doctest)
  unset(CMAKE_POLICY_VERSION_MINIMUM)
endif()

set(DECART_TEST_SOURCES
  test_main.cpp
  test_models.cpp
  test_base64.cpp
  test_enums.cpp)

# The signaling message codec only exists in realtime builds.
if(DECART_BUILD_REALTIME)
  list(APPEND DECART_TEST_SOURCES test_messages.cpp)
endif()

add_executable(decart_tests ${DECART_TEST_SOURCES})

target_include_directories(decart_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src) # internal headers under test (messages, base64)

target_link_libraries(decart_tests PRIVATE
  decart::decart
  doctest::doctest
  nlohmann_json::nlohmann_json)

decart_copy_livekit_runtime(decart_tests)

add_test(NAME decart_tests COMMAND decart_tests)
