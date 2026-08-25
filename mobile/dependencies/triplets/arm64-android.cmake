# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the triplet name compatible with vcpkg's standard Android layout while
# pinning the platform level used to compile every dependency.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 26)

# Autoconf cannot infer that NDK executables are cross-compiled merely from CC.
# Without an explicit host, configure attempts to run its arm64 probe on the
# x86_64 build runner and exits with status 77 (notably in libiconv).
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
