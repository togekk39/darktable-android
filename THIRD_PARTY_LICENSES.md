<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Third-party licenses

This inventory covers the Phase 1 APK skeleton. The APK currently packages AndroidX Activity, Lifecycle, and Jetpack Compose (Apache-2.0), Kotlin runtime (Apache-2.0), and the new GPL-3.0-or-later native adapter. No prebuilt proprietary native library is included.

The upstream tree also contains or references RawSpeed (LGPL-2.1-or-later), LibRaw (LGPL-2.1 or CDDL-1.0 dual license), Khronos OpenCL headers (Apache-2.0), whereami (WTFPL), libxcf (LGPL), Lua/LuaAutoC (MIT), and PhotoSwipe assets. These are **not yet packaged by this skeleton**. Before linking the real RAW slice, dependency source revisions, notices, chosen license options, and transitive Android artifacts must be regenerated and audited. See `.gitmodules`, `src/external/LibRaw-cmake/LICENSE`, `src/external/LuaAutoC/LICENSE.md`, and `data/pswp/LICENSE`.
