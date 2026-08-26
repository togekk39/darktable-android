# SPDX-License-Identifier: GPL-3.0-or-later

# Use vcpkg's Android toolchain as the single source of ABI compiler flags.  In
# particular, do not chainload the NDK toolchain directly: vcpkg's wrapper maps
# this architecture to arm64-v8a before invoking the NDK toolchain.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 26)
