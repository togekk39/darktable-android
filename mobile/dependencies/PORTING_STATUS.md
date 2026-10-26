<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android port status — 2026-10-24

The Android port remains incomplete. The experimental ARM64/API 26 root build
now compiles, links and installs the configured desktop engine, JNI adapter and
modules using real GTK3 Broadway and Android Bionic. Its installed runtime has
been staged and checked. An experimental engine-enabled development APK has
also been assembled and passed final runtime byte/hash verification. No Android
device installation, native loading or RAW/GPR editing/export execution has
been verified. The default Gradle engine remains disabled.

The following results were obtained in the new environment, rather than copied
from the previous environment's test report:

| Check | Result | Scope |
| --- | --- | --- |
| Network policy | Enforced, including the five requested source hosts; actual downloads succeeded | Proxy and TLS checks retained; source mirrors retain the pinned archive hashes |
| Android unit tests | 42 passed, no failures/errors/skips | Kotlin/Compose and adapter boundaries; not native Android execution |
| Android lint | 0 errors, 10 existing warnings | `lintDevDebug` |
| Desktop-backed mobile engine tests | 23 passed | Six original official GPR samples through RawSpeed, full-resolution PNG export/redecode, Kelvin/tint pixels and undo/redo; final core/modules were rebuilt after the signal macro fix described below |
| Offline staging/APK verifier tests | 20 passed | Synthetic ELF/ZIP fixtures exercise validation, including pinned application native libraries; they do not simulate the engine |
| Shared white-balance calculation | 35,057 Kelvin/tint pairs match the original desktop calculations | XYZ values are byte-identical and the inverse float results match; Kotlin contains no replacement formula |
| Effective Android API guard | API 26 accepted with actual compiler macro checks; API 27 rejected | Handles NDK r27's legacy `CMAKE_SYSTEM_VERSION=1` sentinel without relaxing the API baseline |
| GTK/GDK probe | Whole-archive link with undefined symbols disallowed passed | AArch64/API 26, no X11/Wayland or POSIX shared-memory entry points |
| Configured Android engine | 107 core/JNI/module libraries built, linked and installed | Core + JNI + 93 image operations + 7 formats + 5 storage modules |
| Real runtime stage | 109 native libraries and 769 data files verified | Adds NDK `libc++_shared.so` and `libomp.so`; matching darktable data and Lensfun database retained |
| Final development APK | Assemble and final runtime verifier passed | 109 engine/runtime libraries, 107 configured targets, 769 data files and one separately pinned AndroidX application native library; 110 native libraries in total |
| APK signature and manifest | SDK signature verification passed using v2 signing; ARM64, minimum API 26, target API 35 | Development/debug certificate; signature and manifest checks do not establish installation or native execution |
| Native load alignment | Every staged library and all 110 final APK libraries have 16 KiB `PT_LOAD` alignment | This is ELF validation, not execution on a 4 KiB or 16 KiB Android device |

The verified experimental APK has SHA-256
`c4c190c469f9861da502543ecb2fa42b5534d9fb2bbb51d14756fbe4d4e85573`.
The development APK is retained in the execution workspace at
`work/android-build/_app/outputs/apk/dev/debug/app-dev-debug.apk`, with a
curated copy and checksum under `work/deliverables/`. These generated binaries
and build/test caches are not committed to Git.

Its separate application native dependency is the pinned AndroidX
`graphics-path:1.0.1` library described in the
[prebuilt engine documentation](../../android/PREBUILT_ENGINE.md). It is not
counted as a darktable engine module or merged into the engine stage.

The Bionic build also required a compile-time correction to the
`g_signal_connect` return-type guard in `src/gui/gtk.h`: compiler builtins bypass
Bionic's fortified `strlen` wrapper, preserving constant-expression validation.
Fourteen positive/negative compile cases passed with host GCC and NDK Clang.
The Android native graph was built with this correction. A fresh desktop
rebuild completed 354 incremental tasks after that macro/CMake change, and all
23 RAW/engine tests passed again in 67.26 seconds. Compile and host regression
checks do not replace Android execution.

The [manual engine workflow](../../.github/workflows/android-engine.yml) records
the native build, fresh install, staging, explicit Gradle prebuilt route, final
APK runtime audit, signature check and native alignment audit. Its definition
passed actionlint, shell syntax checks and an ELF alignment positive/negative
check. It has not been triggered or validated as a completed GitHub Actions
run. GitHub normally requires a `workflow_dispatch` definition on the default
branch before indexing its manual entry; adding this file on the feature branch
does not change the default branch or establish that the entry is available.
When available, select the intended feature ref when dispatching it.

The configured bootstrap still disables JPEG XL, AVIF/HEIF, JPEG 2000, WebP,
OpenEXR, XCF, G'MIC, AI, OpenCL, Lua, camera tethering and desktop integrations.
The counts above describe that configuration, not all desktop capabilities.
Android font discovery/Fontconfig setup and SVG text watermark rendering also
need runtime integration and validation. Masks/blending controls, multiple
instances, styles, persistent histories, other compound editors and remaining
export settings are still incomplete. A complete camera/format comparison
matrix and device lifecycle, memory, cancellation and RAW-to-export checks
remain required before claiming full desktop parity or a working standalone
Android RAW/GPR editor.
