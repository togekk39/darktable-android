# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED TRIPLET_FILE OR NOT EXISTS "${TRIPLET_FILE}")
  message(FATAL_ERROR "TRIPLET_FILE must identify the Android overlay triplet")
endif()
include("${TRIPLET_FILE}")

function(require_triplet_value name expected)
  if(NOT DEFINED ${name} OR NOT "${${name}}" STREQUAL "${expected}")
    message(FATAL_ERROR "${name} must be '${expected}', got '${${name}}'")
  endif()
endfunction()

require_triplet_value(VCPKG_TARGET_ARCHITECTURE arm64)
require_triplet_value(VCPKG_CMAKE_SYSTEM_NAME Android)
require_triplet_value(VCPKG_CMAKE_SYSTEM_VERSION 26)
require_triplet_value(VCPKG_CMAKE_SYSTEM_PROCESSOR aarch64)
require_triplet_value(ANDROID_ABI arm64-v8a)
require_triplet_value(ANDROID_PLATFORM android-26)
require_triplet_value(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
require_triplet_value(CMAKE_ANDROID_ARCH aarch64)
require_triplet_value(VCPKG_MAKE_BUILD_TRIPLET aarch64-linux-android)
require_triplet_value(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${EXPECTED_NDK_TOOLCHAIN}")

message(STATUS "Validated arm64-v8a / aarch64-linux-android / API 26 triplet")
