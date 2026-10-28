<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android port status — 2026-10-27

The Android port remains incomplete. The experimental ARM64/API 26 root build
now compiles, links and installs the configured desktop engine, JNI adapter and
modules using real GTK3 Broadway and Android Bionic. Its installed runtime has
been staged and checked. An experimental engine-enabled development APK has
also been assembled and passed final runtime byte/hash verification. Installation,
JNI/core loading, runtime extraction and opening one original Fusion GPR have
now run on an official ARM64/API 26 Android emulator. That run crashes at its
first pixel preview in a lowpass OpenMP LUT loop. A separate instruction
diagnostic now passes in an API 28 guest when the host emulator's single-step
mode is enabled; the full JNI editing/export test in that mode is still pending.
No Android editing/export run has passed. The default Gradle engine remains
disabled.

The following recorded results distinguish local build checks, GitHub CI and
actual Android execution; they do not all establish the same scope:

| Check | Result | Scope |
| --- | --- | --- |
| Network policy | Enforced, including the five requested source hosts; all five returned proxy CONNECT 200 on fresh 2026-10-27 HEAD checks | Four source responses were HTTP 200; SourceForge returned HTTP 301 without following the redirect. Earlier dependency downloads retained proxy/TLS checks and pinned archive hashes |
| Android unit tests | 47 passed, no failures/errors/skips | Includes five new instance capability/selector/selection regressions; Kotlin/Compose and adapter boundaries, not native Android execution |
| Android lint | 0 errors, 10 existing warnings | `lintDevDebug` |
| Desktop-backed mobile engine tests | 25 passed in 165.67 seconds after the instance changes | Six original official GPR samples through RawSpeed, full-resolution PNG export/redecode, Kelvin/tint pixels and undo/redo; desktop/Android resource resolver fixtures and original core/modules rebuilt |
| New default module instances | Native host regression passed | Original creation/order/default helpers; independent parameters, exact pixels and topology undo/redo, stable IDs, stale-ID rejection and 64-step history. Linux cleanup instrumentation preserves retained redo objects and confirms reclamation of 1 + 80 abandoned instances; Android execution remains pending |
| Offline staging/APK verifier tests | 23 passed | Synthetic ELF/ZIP fixtures exercise validation, including pinned application native libraries and required original translation catalogs; they do not simulate the engine |
| GitHub CI at `80bb9d3` | All three jobs passed | Desktop Release 25/25, Debug 25/25, standalone host 2/2, runtime tooling 23/23, Android lint/unit tasks and dev APK assembly; the CI APK uses the default dependency-only route |
| Shared white-balance calculation | 35,057 Kelvin/tint pairs match the original desktop calculations | XYZ values are byte-identical and the inverse float results match; Kotlin contains no replacement formula |
| Effective Android API guard | API 26 accepted with actual compiler macro checks; API 27 rejected | Handles NDK r27's legacy `CMAKE_SYSTEM_VERSION=1` sentinel without relaxing the API baseline |
| GTK/GDK probe | Whole-archive link with undefined symbols disallowed passed | AArch64/API 26, no X11/Wayland or POSIX shared-memory entry points |
| Configured Android engine | 107 core/JNI/module libraries built, linked and installed | Core + JNI + 93 image operations + 7 formats + 5 storage modules |
| Real runtime stage | 109 native libraries and 805 data files verified | Adds NDK `libc++_shared.so` and `libomp.so`; matching darktable data, 36 original translation catalogs and Lensfun database retained |
| Final development APK | Assemble and final runtime verifier passed | 109 engine/runtime libraries, 107 configured targets, 805 data files and one separately pinned AndroidX application native library; 110 native libraries in total |
| Compact installation APK | 40,413,700 bytes; signature and final runtime verifier passed | Fresh CMake `--strip` installation reduces configured libraries from 241,128,512 to 45,914,616 bytes. All allocated sections, including `.text` and `.rodata`, and all load-segment fields match the original 107 libraries exactly |
| New default-instance APK at local `e084d6f085` | 40,423,928 bytes; build, signature and final runtime verifier passed | Fresh rebuilt 107 targets, 109 staged libraries, 805 data files and 110 final APK libraries; all 2,311 allocated sections and load/dynamic metadata retained exactly through stripping. Paired instrumentation APK contains the new instance checks; their Android execution remains pending |
| APK signature and manifest | SDK signature verification passed using v2 signing; ARM64, minimum API 26, target API 35 | Development/debug certificate; signature and manifest checks do not establish installation or native execution |
| Native load alignment | Every staged library and all 110 final APK libraries have 16 KiB `PT_LOAD` alignment | This is ELF validation, not execution on a 4 KiB or 16 KiB Android device |
| Android installation and JNI/core loading | Passed on ARM64/API 26 emulator | Signed APK installation, real native capability mask 3 and private runtime extraction/configuration |
| Android original Fusion GPR open | Passed | `GPBK7066.GPR`, source SHA-256 `c03eccdc334dc5b35cf6385799b219f4c9a0bc18ccbe70fe433237d98f352247`; native open returned a live session |
| Initial API 26 JNI preview/edit/export run | Failed at first preview | SIGSEGV in `lowpass.c` brightness LUT's OpenMP outlined loop; that run stopped before exposure, Kelvin/tint, undo/redo and full-size export |
| API 28 instruction diagnostic | Passed with host `singlestep on`; failed in normal host mode | Same original executable and Android image; all 65,536 values exact, allocation canaries intact, exit 0 in single-step mode. This is not the JNI engine test |
| API 28 framework boot | Completed with host `singlestep on` | `sys.boot_completed=1`, boot animation exited successfully and SELinux remained enforcing; full engine JNI execution in this mode is still pending |
| API 28 JNI original preview and exposure | Passed in the preserved compact APK run | Fixed host instruction timing and single-step mode; actual original Fusion GPR preview returned, exposure changed pixels, and undo/redo restored exact previews. Kelvin/tint, export and final close/reopen checks are still running |

The compact installation APK has SHA-256
`b4ebf6195869ae0f80f42dffda21839c66271a3d841ef67b725cc3327ed7ce72`.
It is retained at `work/deliverables/darktable-android-engine-stripped-dev-debug.apk`,
with a matching checksum and build audit. The full-symbol APK
(`e6f045f7a07b2851f775b96ab91330b87576b985cc617264d3ca42731cedfaaf`)
remains separately at `work/deliverables/darktable-android-engine-dev-debug.apk`;
its matching runtime manifest and native audit also remain there. Original
build libraries retain their debug symbols. The older pre-resource-fix APK is
`darktable-android-engine-before-private-path-fix.apk`. Generated binaries and
build/test caches are not committed to Git.

The newer default-instance APK is
`work/deliverables/darktable-android-engine-instances-dev-debug.apk`, SHA-256
`734cfbdafff4cbac0f69cd7fb00f6ff5e0d25f61dd0044d80ad441be4840d225`.
Its paired test APK has SHA-256
`228022cacd52d795d5e36002d184883dfb1bb5594e04e16fb32545a50f289b20`.
Both signatures, the exact runtime bytes and native alignment were verified;
the new instance feature has not yet run on Android.

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
missing catalogs before desktop initialization. In the initial API 26 test,
the corrected APK opened the original GPR, then failed in the first preview;
that run was not a successful editor execution test.

The first JNI test used the unmodified official Google APIs ARM64/API 26
revision 3 image under official Emulator 30.8.4 with TCG on an x86_64 host (no
KVM). A standalone copy of the vector instruction sequence also crashes without
OpenMP or libm. Individual DUP instructions, simple zero-vector store controls
and a 24-case `powf` register-preservation probe pass. These controls do not
identify a single faulty instruction.

Official Emulator 37.2.12 also reproduces the standalone failure with the API 26
image. Its QMP version reports QEMU `2.12.0-19097-g85fa07f04ef`; changing the SDK
emulator release did not provide an independent modern QEMU implementation.
The unmodified official AOSP API 28 ARM64 image, with kernel 4.4.302, likewise
fails the same diagnostic in normal host mode. Booting these images through the
raw backend required explicit host device trees matching the MMIO disks and
API 28 system-as-root layout. Android kernel, ramdisk, system and vendor image
bytes, authentication settings and SELinux policies were not changed.

On 2026-10-27, QMP/HMP `singlestep on` changed only the host execution mode.
The same API 28 guest and original dynamic diagnostic executable then returned
exit 0, preserved its allocation canaries and produced all 65,536 values
exactly. The API 28 framework also reached `sys.boot_completed=1`, with a
successful boot-animation exit and SELinux enforcing. The full JNI smoke test
in this mode remains pending. An independent
Debian QEMU 10.0.13 user-mode run also passes using that exact executable and
the original API 28 Bionic linker/libc/libdl files; user-mode execution is not
Android guest or darktable execution proof. Together these results narrow the
investigation to execution-context differences without proving a specific
translation fault. No production algorithm or vectorization flags were changed.
A subsequent cold boot using host `-singlestep` and fixed instruction timing
`-icount shift=0,align=off,sleep=on` reached framework boot completion and
SELinux enforcing. The original vector diagnostic passed again, both earlier
compact APKs installed successfully, and the real JNI test reached native
capabilities, runtime extraction/configuration and the original GPR open phase.
The run is still in progress; it is using the preserved earlier APK pair, not
the new instance APK. Earlier single-step installation attempts timed out;
the host timing settings must accompany any eventual result. They do not alter
guest image bytes or production processing algorithms and establish no device
performance claim.
See
[NATIVE_ENGINE_TESTS.md](../../android/NATIVE_ENGINE_TESTS.md) for the actual
JNI test, required original fixture and pass criteria. Building the test APK
alone is not execution proof.

The [successful GitHub CI run](https://github.com/togekk39/darktable-android/actions/runs/40537005151)
at `80bb9d3` independently completed desktop Release and Debug tests in 66.24
and 244.06 seconds respectively. It also passed the standalone host and offline
runtime tests, plus `lintDevRelease`, `testDevReleaseUnitTest` and
`assembleDevRelease`. This run verifies the normal dependency-only Android
build; its artifact does not contain the complete processing engine.

The [engine APK workflow](../../.github/workflows/android-engine.yml) now builds
native/runtime changes on `feature/android-raw-editor`, as well as retaining
manual dispatch. It records the native build, fresh stripped installation,
staging, explicit Gradle prebuilt route, final APK runtime audit, signature
check and native alignment audit, then uploads the main and instrumentation
APKs. Its definition passed actionlint and shell syntax checks; a completed
full-engine GitHub Actions build has not yet been verified. Branch push events
avoid depending on indexing a manual entry on the default branch. This does
not modify the default branch or enable the normal dependency-only Gradle
engine. Documentation-only changes are excluded from this expensive build.
The updated workflow is currently committed locally. GitHub reads remain working,
but `create_tree` and then a narrower `create_blob` call produced no response and
were interrupted; neither produced its expected object. No commit/ref write was
attempted, no rejection reason was returned, and the remote remains `80bb9d38`.
The new full-engine workflow has therefore not been triggered remotely.

The configured bootstrap still disables JPEG XL, AVIF/HEIF, JPEG 2000, WebP,
OpenEXR, XCF, G'MIC, AI, OpenCL, Lua, camera tethering and desktop integrations.
The counts above describe that configuration, not all desktop capabilities.
Android font discovery/Fontconfig setup and SVG text watermark rendering also
need runtime integration and validation. Creating default module instances is
implemented with host-tested topology undo/redo; copying current settings/masks,
deleting and reordering instances remain unavailable. Masks/blending controls,
styles, persistent histories, other compound editors and remaining
export settings are still incomplete. A complete camera/format comparison
matrix and device lifecycle, memory, cancellation and RAW-to-export checks
remain required before claiming full desktop parity or a working standalone
Android RAW/GPR editor.
