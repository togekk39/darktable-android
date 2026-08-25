# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the triplet name compatible with vcpkg's standard Android layout while
# pinning the platform level used to compile every dependency.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 26)
