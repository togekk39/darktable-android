# SPDX-License-Identifier: GPL-3.0-or-later

# The pinned libiconv port uses vcpkg_configure_make, which forwards this value
# verbatim. GNU configure only enables cross-compilation for an explicit
# --host option; a bare canonical tuple still attempts to run target probes.
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")

# Retain the source, checksums, patches, and packaging logic from the immutable
# catalog instead of duplicating them in this narrowly scoped compatibility fix.
set(CURRENT_PORT_DIR "${VCPKG_ROOT_DIR}/ports/libiconv")
include("${VCPKG_ROOT_DIR}/ports/libiconv/portfile.cmake")
