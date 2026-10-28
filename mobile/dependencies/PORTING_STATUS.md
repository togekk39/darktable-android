<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android port status — 2026-10-28

The Android port remains incomplete. The experimental ARM64/API 26 root build
now compiles, links and installs the configured desktop engine, JNI adapter and
modules using real GTK3 Broadway and Android Bionic. Its installed runtime has
been staged and checked. An experimental engine-enabled development APK has
also been assembled and passed final runtime byte/hash verification. Installation,
JNI/core loading, runtime extraction and opening one original Fusion GPR have
run on an official ARM64/API 26 Android emulator; its initial normal-mode preview
failed. A preserved compact APK pair subsequently passed the complete Fusion
GPR JNI edit/undo/full-size PNG export/source-protection/reopen test on ARM64/API 28
with host single-step emulation and fixed virtual timing. The newer default-instance
APK also passed its complete 41-phase run. This is one original fixture/runtime, not complete
format/feature parity, physical-device validation or performance evidence.
The default Gradle engine remains disabled.

## XCF export increment after `90ce43520f`

Local source `d866419cb5` enables the original pinned libxcf export module and
exposes its original 8-bit unsigned, 16-bit unsigned and 32-bit float settings
through native metadata and the existing Android export controls. No replacement
pixel or compression algorithm is used. LibRaw, RawSpeed and libxcf gitlinks are
unchanged. A build-generated copy fixes libxcf error cleanup/buffered-close
status, and the original format module propagates failures. The host engine
regression passed with XCF presence required, including real kernel-enforced
write failure, exact configuration restoration and destination preservation.
The host Release/Debug CI configuration now enables XCF for this regression;
the successful `90ce43520f` CI results below predate this change.

The ARM64/API 26 native build, fresh stripped install and prebuilt Gradle APK
passed. There are now 108 configured libraries, 110 staged libraries, 805 data
files and 111 final APK libraries. All final native libraries have 16 KiB load
alignment; both APKs passed v2 signature verification and final runtime hashes.
The compact APK has no unused ZIP holes. The actual tested pair is:

* Main APK: 40,440,735 bytes, SHA-256
  `8a564200bb31a006d2b93b685d3635841c379cc5913484336a838575492b4d90`.
* Test APK: 395,503 bytes, SHA-256
  `a1a9b993d24d9750995c7186984face0a33662802c4d766b647ef86a0792c515`.

`NativeXcfExportTest` passed all 18 phases on the official ARM64/API 28 guest
under host single-step/fixed-clock emulation. It opened official Fusion
`GPBK7066.GPR`, changed native exposure, exported three 128x123 XCF/TIFF pairs
and closed the session. Source SHA-256 remained
`c03eccdc334dc5b35cf6385799b219f4c9a0bc18ccbe70fe433237d98f352247`.
GIMP 3.0.4 independently decoded all six pulled/hash-verified outputs. Complete
native RGB bytes and embedded sRGB profiles matched each pair exactly at 8-bit,
16-bit and float32 precision; all floats were finite. Native RGB SHA-256 values:

* UINT8 (47,232 bytes): `0726832947204304c1ff2a60382cdb5bcfd018d638785f370d1d8bb27f34804c`.
* UINT16 (94,464 bytes): `964b6bfbff1060600e73c9d579eb3100e2fff0ced69ef2fd655ef1a55275980b`.
* FLOAT32 (188,928 bytes): `62a98e440cf102e8c320f8d3fad7da7811d0d7f7b8c71257425fabd942278bb7`.

Independent host exports at 192x185 also matched paired TIFF pixels/ICC in GIMP
at all three precisions. Work-only evidence includes `work/logs/xcf-android-jni-run.log`,
`work/xcf-validation/{android-result,android-gimp-report,host-gimp-report}.json`
and `work/logs/xcf-android-apk-audit.json`. The older table below records the
pre-XCF configurations. Full-resolution XCF, XCF import, masks/channels,
physical devices and complete desktop parity remain unverified.

The following recorded results distinguish local build checks, GitHub CI and
actual Android execution; they do not all establish the same scope:

| Check | Result | Scope |
| --- | --- | --- |
| Network policy | Enforced, including the five requested source hosts; all five returned proxy CONNECT 200 on fresh 2026-10-27 HEAD checks | Four source responses were HTTP 200; SourceForge returned HTTP 301 without following the redirect. Earlier dependency downloads retained proxy/TLS checks and pinned archive hashes |
| Android unit tests | 47 passed, no failures/errors/skips | Includes five new instance capability/selector/selection regressions; Kotlin/Compose and adapter boundaries, not native Android execution |
| Android lint | 0 errors, 10 existing warnings | `lintDevDebug` |
| Desktop-backed mobile engine tests | 25 passed in 165.67 seconds after the instance changes | Six original official GPR samples through RawSpeed, full-resolution PNG export/redecode, Kelvin/tint pixels and undo/redo; desktop/Android resource resolver fixtures and original core/modules rebuilt |
| New default module instances | Native host and Android exposure-instance checks passed | Original creation/order/default helpers; independent parameters, exact pixels and topology undo/redo, stable IDs and stale-ID rejection. Host checks additionally cover 64-step history and actual reclamation of 1 + 80 abandoned instances while retaining redo objects |
| Offline staging/APK verifier tests | 23 passed | Synthetic ELF/ZIP fixtures exercise validation, including pinned application native libraries and required original translation catalogs; they do not simulate the engine |
| GitHub CI at `80bb9d3` | All three jobs passed | Desktop Release 25/25, Debug 25/25, standalone host 2/2, runtime tooling 23/23, Android lint/unit tasks and dev APK assembly; the CI APK uses the default dependency-only route |
| GitHub CI at `90ce43520f` | All three jobs passed | Fresh remote Release 25/25 (72.23 seconds), Debug 25/25 (467.08 seconds), standalone host 2/2, runtime tooling 23/23, Android lint/unit tasks and dependency-only dev release APK assembly |
| Full-engine GitHub workflow at `90ce43520f` | Complete build and audited main/test APK artifact passed | Native core/JNI/configured modules, fresh stripped installation, runtime staging, final APK hashes, signatures and 16 KiB load alignment; CI did not execute Android instrumentation |
| Shared white-balance calculation | 35,057 Kelvin/tint pairs match the original desktop calculations | XYZ values are byte-identical and the inverse float results match; Kotlin contains no replacement formula |
| Effective Android API guard | API 26 accepted with actual compiler macro checks; API 27 rejected | Handles NDK r27's legacy `CMAKE_SYSTEM_VERSION=1` sentinel without relaxing the API baseline |
| GTK/GDK probe | Whole-archive link with undefined symbols disallowed passed | AArch64/API 26, no X11/Wayland or POSIX shared-memory entry points |
| Configured Android engine | 107 core/JNI/module libraries built, linked and installed | Core + JNI + 93 image operations + 7 formats + 5 storage modules |
| Real runtime stage | 109 native libraries and 805 data files verified | Adds NDK `libc++_shared.so` and `libomp.so`; matching darktable data, 36 original translation catalogs and Lensfun database retained |
| Final development APK | Assemble and final runtime verifier passed | 109 engine/runtime libraries, 107 configured targets, 805 data files and one separately pinned AndroidX application native library; 110 native libraries in total |
| Compact installation APK | 40,413,700 bytes; signature and final runtime verifier passed | Fresh CMake `--strip` installation reduces configured libraries from 241,128,512 to 45,914,616 bytes. All allocated sections, including `.text` and `.rodata`, and all load-segment fields match the original 107 libraries exactly |
| New default-instance APK at local `e084d6f085` | 40,423,928 bytes; build, signature, runtime verifier and complete Fusion JNI run passed | Fresh rebuilt 107 targets, 109 staged libraries, 805 data files and 110 final APK libraries; all 2,311 allocated sections and load/dynamic metadata retained exactly through stripping. Android execution scope is the fixture/runtime recorded below |
| APK signature and manifest | SDK signature verification passed using v2 signing; ARM64, minimum API 26, target API 35 | Development/debug certificate; signature and manifest checks do not establish installation or native execution |
| Native load alignment | Every staged library and all 110 final APK libraries have 16 KiB `PT_LOAD` alignment | This is ELF validation, not execution on a 4 KiB or 16 KiB Android device |
| Android installation and JNI/core loading | Passed on ARM64/API 26 emulator | Signed APK installation, real native capability mask 3 and private runtime extraction/configuration |
| Android original Fusion GPR open | Passed | `GPBK7066.GPR`, source SHA-256 `c03eccdc334dc5b35cf6385799b219f4c9a0bc18ccbe70fe433237d98f352247`; native open returned a live session |
| Initial API 26 JNI preview/edit/export run | Failed at first preview | SIGSEGV in `lowpass.c` brightness LUT's OpenMP outlined loop; that run stopped before exposure, Kelvin/tint, undo/redo and full-size export |
| API 28 instruction diagnostic | Passed with host `singlestep on`; failed in normal host mode | Same original executable and Android image; all 65,536 values exact, allocation canaries intact, exit 0 in single-step mode. This is not the JNI engine test |
| API 28 framework boot | Completed with host `singlestep on` | `sys.boot_completed=1`, boot animation exited successfully and SELinux remained enforcing; the later complete JNI run is recorded below |
| API 28 JNI original preview and exposure | Passed in the preserved compact APK run | Fixed host instruction timing and single-step mode; actual original Fusion GPR preview returned, exposure changed pixels, and undo/redo restored exact previews |
| API 28 complete Fusion JNI run | Passed; runner `OK (1 test)` and report `status: passed` | Preserved main `b4ebf619…` / test `688b5d44…`; native capabilities 3, real exposure and Kelvin/tint pixels, exact undo/redo, Android-decodable 3104×3000 PNG, unchanged source after final close, and reopened preview exact. Host `-singlestep -icount shift=0,align=off,sleep=on`; the new default-instance APK remains a separate run |
| API 28 complete default-instance Fusion run | Passed; runner `OK (1 test)`, report `status: passed`, 41 completed phases | Main `734cfbda…` / test `228022ca…`; adds `exposure@1`, independent exposure pixels, edit and topology undo/redo, retained ID and retired-selector rejection, then completes Kelvin/tint, full-size PNG, source protection and reopen checks under the same host settings |
| Full-resolution output regression | All 27,936,000 RGB bytes identical across both APK runs | Independently decoded 8-bit 3104×3000 PNGs; pixel SHA-256 `7d1e6eaf7133d3db50bf857ac59033553338f0285104c30f15944d635dc18f66`. Initial previews also match exactly; PNG file/metadata bytes differ |

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
Both signatures, exact runtime bytes and native alignment were verified. Its
complete Android run passed with the recorded host settings. The actual output
is retained at `work/deliverables/android-fusion-instances-edited-full-size.png`,
14,830,806 bytes, SHA-256
`7f0a8fdc81b7fdba246d51a0a0d16fbec4d71c1aa4bff55fe3f0413c2a2dac1b`.
The source GPR remained unchanged after final close; the saved report and output
comparison are retained beside the artifact.

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
successful boot-animation exit and SELinux enforcing. That initial diagnostic
run did not establish full JNI execution. An independent
Debian QEMU 10.0.13 user-mode run also passes using that exact executable and
the original API 28 Bionic linker/libc/libdl files; user-mode execution is not
Android guest or darktable execution proof. Together these results narrow the
investigation to execution-context differences without proving a specific
translation fault. No production algorithm or vectorization flags were changed.
A subsequent cold boot using host `-singlestep` and fixed instruction timing
`-icount shift=0,align=off,sleep=on` reached framework boot completion and
SELinux enforcing. The original vector diagnostic passed again, both earlier
compact APKs installed successfully, and the real JNI test reached native
capabilities, runtime extraction/configuration and all edit/export/reopen checks.
The runner reported `OK (1 test)` and the saved report has `status: passed`.
The resulting Android-decoded PNG is 3104×3000, 14,830,811 bytes, SHA-256
`e8b53d300d6bf293ae9ed32cd49a27446c09bca5dfc4a78ee4767a326fdb1fd6`.
The original GPR hash remained unchanged after final close, and reopened preview
pixels matched exactly. This used the preserved earlier APK pair. The newer
default-instance pair then passed its own full 41-phase run and the independent
full-resolution pixel comparison recorded above.
Earlier single-step installation attempts timed out;
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
APKs. The complete
[remote run at `90ce43520f`](https://github.com/togekk39/darktable-android/actions/runs/40722614711)
passed all these steps. Its artifact `12833671749`,
`experimental-android-engine-90ce43520fd375ca7bf08663505022a68ca9e876`,
is a 41,472,523-byte archive with SHA-256
`f5eebdc10f1f05fea40e177fab72ddee65be7dd158049ce8ea1a4fbd227993fe`.
It contains the signed engine APK, paired instrumentation APK, source commit,
checksums and runtime/native audit reports. The final main APK runtime audit
reports SHA-256 `21e0e173e39c911e4a3fab3862bad1256b6ab17f2802003d685e5e9605873019`,
107 configured libraries, 109 stage libraries, one separately pinned application
library and 805 assets. This is the recorded pre-XCF configuration. The job does not install or execute
Android tests, so this is separate from the locally executed API 28 pair above.
The [normal Android CI run](https://github.com/togekk39/darktable-android/actions/runs/40722614405)
at the same commit independently passed all three jobs; its dev release remains
the dependency-only APK. Full logs and artifact metadata are retained in
`work/logs/ci90-github-summary.json` and the corresponding `ci90-job-*.log` files.
Branch push events avoid depending on indexing a manual entry on the default
branch. This does
not modify the default branch or enable the normal dependency-only Gradle
engine. Documentation-only changes are excluded from this expensive build.
GitHub publication through the explicit `togekk39` connection subsequently
succeeded. All five formerly local commits were recreated and the feature ref
advanced from `80bb9d38` to `90ce43520f` without force. Every recreated commit's
tree matches its original local commit tree exactly. Original commits remain in
the local backup branch and source bundle; the work branch now follows the
published history. The recorded publication mapping is
`work/logs/github-publication-success-2026-10-28.json`.

The verified `90ce43520f` bootstrap disables JPEG XL, AVIF/HEIF, JPEG 2000, WebP,
OpenEXR, XCF, G'MIC, AI, OpenCL, Lua, camera tethering and desktop integrations.
The counts above describe that configuration, not all desktop capabilities.
The [format coverage audit](FORMAT_PARITY.md) separates input decoders, actual
processed-output modules, optional RAW subtypes and unexposed export settings.
Android font discovery/Fontconfig setup and SVG text watermark rendering also
need runtime integration and validation. Creating default module instances is
implemented with host-tested topology undo/redo; copying current settings/masks,
deleting and reordering instances remain unavailable. Masks/blending controls,
styles, persistent histories, other compound editors and remaining
export settings are still incomplete. A complete camera/format comparison
matrix and device lifecycle, memory, cancellation and RAW-to-export checks
remain required before claiming full desktop parity or a working standalone
Android RAW/GPR editor.
