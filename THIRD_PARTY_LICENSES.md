<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Third-party licenses

This inventory covers the Phase 1 APK. The APK packages AndroidX Activity, Lifecycle, and Jetpack Compose (Apache-2.0), Kotlin runtime (Apache-2.0), the GPL-3.0-or-later native adapter, and the source-built native bundle listed below. No prebuilt proprietary native library is included.

The upstream tree also contains or references Khronos OpenCL headers (Apache-2.0), whereami (WTFPL), libxcf (LGPL), Lua/LuaAutoC (MIT), and PhotoSwipe assets. Those optional desktop sources are not packaged in the Android APK. See `.gitmodules`, `src/external/LibRaw-cmake/LICENSE`, `src/external/LuaAutoC/LICENSE.md`, and `data/pswp/LICENSE`.
# Android native dependency bundle

The Android build statically links the pinned RawSpeed, Exiv2, GLib, SQLite,
Little CMS 2, zlib, libjpeg-turbo, libpng, libtiff, pugixml and LibRaw sources.
Their versions, provenance, licenses, link form, archive verification mechanism
and redistribution notes are maintained in `mobile/dependencies/README.md`.
