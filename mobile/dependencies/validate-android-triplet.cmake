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
require_triplet_value(VCPKG_MAKE_BUILD_TRIPLET --host=aarch64-linux-android)

if(NOT DEFINED VCPKG_CMAKE_CONFIGURE_OPTIONS)
  message(FATAL_ERROR "VCPKG_CMAKE_CONFIGURE_OPTIONS must define the Android ABI")
endif()
list(FIND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DANDROID_ABI=arm64-v8a" android_abi_option_index)
if(android_abi_option_index EQUAL -1)
  message(FATAL_ERROR
    "VCPKG_CMAKE_CONFIGURE_OPTIONS must contain '-DANDROID_ABI=arm64-v8a', got '${VCPKG_CMAKE_CONFIGURE_OPTIONS}'")
endif()

# These settings bypass or override vcpkg's architecture mapping and have
# previously allowed stale ARMv7 flags to leak into make-based ports.
foreach(name IN ITEMS ANDROID_ABI ANDROID_PLATFORM CMAKE_ANDROID_ARCH_ABI
                      CMAKE_ANDROID_ARCH VCPKG_CMAKE_SYSTEM_PROCESSOR
                      VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
  if(DEFINED ${name})
    message(FATAL_ERROR "${name} must be derived by vcpkg's Android toolchain")
  endif()
endforeach()

message(STATUS "Validated vcpkg arm64 Android / API 26 triplet")
