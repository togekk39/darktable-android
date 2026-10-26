<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android port status — 2026-10-26

The Android port remains incomplete. The experimental ARM64/API 26 root build
now compiles, links and installs the configured desktop engine, JNI adapter and
modules using real GTK3 Broadway and Android Bionic. Its installed runtime has
been staged and checked. An experimental engine-enabled development APK has
also been assembled and passed final runtime byte/hash verification. Installation,
JNI/core loading, runtime extraction and opening one original Fusion GPR have
now run on an official ARM64/API 26 Android emulator. The first pixel preview
crashes in a lowpass OpenMP LUT loop, so Android editing/export execution has
not passed. The default Gradle engine remains disabled.

The following results were obtained in the new environment, rather than copied
from the previous environment's test report:

| Check | Result | Scope |
| --- | --- | --- |
| Network policy | Enforced, including the five requested source hosts; actual downloads succeeded | Proxy and TLS checks retained; source mirrors retain the pinned archive hashes |
| Android unit tests | 42 passed, no failures/errors/skips | Kotlin/Compose and adapter boundaries; not native Android execution |
| Android lint | 0 errors, 10 existing warnings | `lintDevDebug` |
| Desktop-backed mobile engine tests | 25 passed | Six original official GPR samples through RawSpeed, full-resolution PNG export/redecode, Kelvin/tint pixels and undo/redo; adds desktop/Android resource resolver fixtures, with final core/modules rebuilt |
| Offline staging/APK verifier tests | 23 passed | Synthetic ELF/ZIP fixtures exercise validation, including pinned application native libraries and required original translation catalogs; they do not simulate the engine |
| Shared white-balance calculation | 35,057 Kelvin/tint pairs match the original desktop calculations | XYZ values are byte-identical and the inverse float results match; Kotlin contains no replacement formula |
| Effective Android API guard | API 26 accepted with actual compiler macro checks; API 27 rejected | Handles NDK r27's legacy `CMAKE_SYSTEM_VERSION=1` sentinel without relaxing the API baseline |
| GTK/GDK probe | Whole-archive link with undefined symbols disallowed passed | AArch64/API 26, no X11/Wayland or POSIX shared-memory entry points |
| Configured Android engine | 107 core/JNI/module libraries built, linked and installed | Core + JNI + 93 image operations + 7 formats + 5 storage modules |
| Real runtime stage | 109 native libraries and 805 data files verified | Adds NDK `libc++_shared.so` and `libomp.so`; matching darktable data, 36 original translation catalogs and Lensfun database retained |
| Final development APK | Assemble and final runtime verifier passed | 109 engine/runtime libraries, 107 configured targets, 805 data files and one separately pinned AndroidX application native library; 110 native libraries in total |
| APK signature and manifest | SDK signature verification passed using v2 signing; ARM64, minimum API 26, target API 35 | Development/debug certificate; signature and manifest checks do not establish installation or native execution |
| Native load alignment | Every staged library and all 110 final APK libraries have 16 KiB `PT_LOAD` alignment | This is ELF validation, not execution on a 4 KiB or 16 KiB Android device |
| Android installation and JNI/core loading | Passed on ARM64/API 26 emulator | Signed APK installation, real native capability mask 3 and private runtime extraction/configuration |
| Android original Fusion GPR open | Passed | `GPBK7066.GPR`, source SHA-256 `c03eccdc334dc5b35cf6385799b219f4c9a0bc18ccbe70fe433237d98f352247`; native open returned a live session |
| Android preview/edit/export execution test | Failed at first preview | SIGSEGV in `lowpass.c` brightness LUT's OpenMP outlined loop; exposure, Kelvin/tint, undo/redo and full-size export phases have not run on Android |

The verified experimental APK has SHA-256
`e6f045f7a07b2851f775b96ab91330b87576b985cc617264d3ca42731cedfaaf`.
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
25 RAW/engine/resource tests passed again in 98.09 seconds after the Android
resource path changes. Compile and host regression
checks do not replace Android execution.

The first Android execution exposed executable-relative locale/share defaults
pointing beside Android's system `app_process`, causing desktop path lookup to
exit the application. Android now derives these defaults from the configured
private data tree, preserves explicit locale overrides, and imports built-in
styles from the staged `data/styles` directory. Staging retains the original
installed `share/locale` tree, and both Java and native configuration reject
missing catalogs before desktop initialization. The corrected APK opens the
original GPR, then fails in the first preview; this is not a successful editor
execution test.

The test uses the unmodified official Google APIs ARM64/API 26 revision 3 image
under official Emulator 30.8.4 with TCG on an x86_64 host (no KVM). Authentication
settings and system images were not modified. An independent NDK executable
also reproduces the LUT crash with the same NDK `libomp.so`, while the serial
loop succeeds. A standalone copy of the valid vector instruction sequence also
crashes with no OpenMP or libm dependency. Individual DUP instructions and a
24-case `powf` register-preservation probe pass. The contiguous instruction
sequence and emulator execution are now the focus; no production algorithm or
vectorization flags have been changed. See
[NATIVE_ENGINE_TESTS.md](../../android/NATIVE_ENGINE_TESTS.md) for the actual
JNI test, required original fixture and pass criteria. Building the test APK
alone is not execution proof.

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
