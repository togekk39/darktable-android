# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the triplet name compatible with vcpkg's standard Android layout while
# pinning the platform level used to compile every dependency.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 26)
set(VCPKG_CMAKE_SYSTEM_PROCESSOR aarch64)

# Autoconf cannot infer that NDK executables are cross-compiled merely from CC.
# vcpkg-make adds the --host option itself; this variable must contain only the
# canonical tuple.  The explicit CMake processor above also keeps make-based
# ports from falling back to vcpkg's 32-bit ARM Android compiler.
set(VCPKG_MAKE_BUILD_TRIPLET "aarch64-linux-android")
