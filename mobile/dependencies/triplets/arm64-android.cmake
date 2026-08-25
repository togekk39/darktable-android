# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the triplet name compatible with vcpkg's standard Android layout while
# pinning the platform level used to compile every dependency.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 26)
set(VCPKG_CMAKE_SYSTEM_PROCESSOR aarch64)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-26)
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_ARCH aarch64)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")

# Keep Autoconf ports and the NDK toolchain on the same 64-bit Android target.
# vcpkg-make adds the --host option itself, so this is the canonical tuple only.
set(VCPKG_MAKE_BUILD_TRIPLET "aarch64-linux-android")
