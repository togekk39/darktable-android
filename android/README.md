<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android build

Open this `android/` directory in Android Studio. Install Android SDK 35, NDK 27.2.12479018, and CMake 3.22.1. Build `devDebug` or run `./gradlew :app:assembleDevDebug`; install with `adb install -r app/build/outputs/apk/dev/debug/app-dev-debug.apk`.

The Android UI opens one document directly into the editor, with no image browser.
It now includes controls generated from reviewed desktop scalar parameters,
dedicated basecurve and RGB curve node editors, white-balance temperature/tint and coefficients/settings, a freehand crop editor,
session undo/redo, and a system document destination for native exports. The
default Gradle build still sets `DT_MOBILE_WITH_DARKTABLE_ENGINE=OFF`; that
dependency-only route reports processing unavailable and disables image opening.
The experimental ARM64/API 26 core, JNI and configured modules now build/link
with real GTK3 Broadway, and their runtime has been installed and staged.
The explicit [prebuilt route](PREBUILT_ENGINE.md) packages that stage, and an
experimental engine-enabled APK has passed final runtime byte/hash verification.
A preserved APK pair passed the complete JNI Fusion GPR edit/undo/export/reopen
test on ARM64/API 28 with host single-step emulation and fixed virtual timing;
the newer default-instance APK pair is being tested separately. See
[port status](../mobile/dependencies/PORTING_STATUS.md) for exact scope and host
settings. One fixture/runtime does not establish full desktop parity, physical
device behavior or performance.

The numeric bridge deliberately exposes an audited subset: exposure/black
correction and compensation switches, Velvia strength/bias, Lab color contrast,
six global color-balance RGB controls, Sigmoid contrast/skew/hue preservation,
and sharpen amount/threshold. Other modules can still be listed and
enabled, but internal counts, arrays and unreviewed fields require dedicated
editors that maintain the same parameter invariants as the desktop widgets.

Modules advertising native instance support offer **New default instance**.
The original engine creates image-specific default parameters and blending;
the editor focuses the returned instance and edits its independent settings.
One undo removes the instance, and redo restores its stable identifier and settings.
This does not copy the source instance's edits or masks. Copying current settings,
deleting and reordering instances remain pending. Host native tests and Android JVM
tests pass and the new engine APK has been built; this feature's Android run is
still in progress.

The curve editors support 2–20 nodes through dragging or numeric coordinates,
adding/deleting nodes, resetting to identity, and choosing the upstream cubic
spline, Catmull-Rom or monotone Hermite interpolation. Applying a curve submits
its nodes and interpolation together; the displayed samples come from the native
upstream spline implementation. Basecurve exposes its combined channel. RGB
curve exposes the combined channel in linked mode or separate red, green and
blue channels in independent mode. RGB mode and preserve-color choices can be
applied together. Switching to independent mode inherits the shared curve only
when both other channels remain identity curves, matching desktop behavior;
edited channel curves are retained. Middle-gray compensation and other advanced
curve settings, and other curve modules, still need dedicated editors. These controls
require an engine-enabled build; the current dependency-only APK cannot process
or apply them to an image.

White balance applies all active channel coefficients together and offers the
image's original as-shot, camera-reference and as-shot-to-reference settings
when available. Manual coefficients use `0.001..8`; native preset values above
8 remain visible without clipping. Green remains an independent coefficient.
Three-channel images preserve the inactive fourth value; four-Bayer labels and
display order come from the native module. True camera-monochrome inputs do not
offer white balance. When the native module can derive temperature/tint from the
image's camera matrix and active gains, two numeric controls apply Kelvin and tint
together through the desktop temperature module's shared calculation core. Android
does not calculate replacement gains. The native module supplies the current values
and bounds; applying selects user-defined white balance and normalizes green to 1,
matching the desktop controls. Unavailable conversion keeps the existing coefficient
and setting controls. Spot sampling and camera presets remain pending.

The crop editor applies four normalized edges together through the original
crop module, with a rectangle diagram and a reset to the full input area.
Applying selects freehand proportions; a message identifies when this will
replace an existing fixed ratio. The diagram is separate from the cropped
photo preview. Fixed aspect presets and a touch overlay on an uncropped image
are still pending.

Undo/redo tracks up to 64 successful Apply steps during the open image session.
Edits after undo discard redo. Export options come from the installed native
format's capabilities: maximum dimensions without upscaling, PNG/TIFF precision
and compression, JPEG quality/subsampling, JPEG/TIFF DPI, built-in output
profiles, rendering intent and source-metadata copying. The document picker
retains the complete settings and image identity selected when export started.
Source metadata suppression does not remove ICC profiles or all fields generated
by the encoder. Custom ICC files and further codec-specific settings remain
pending. These features require the processing engine to be built and packaged.

The desktop-backed mobile adapter and the GoPro VC-5 decoder have independent
native verification targets. See [mobile engine integration](../mobile/README.md)
for the build options, tests, and remaining work. Do not enable the Android
engine flag until the core, processing/format plugins and runtime assets can
actually be built and initialized on the device.

## Native runtime data

`RuntimeAssets.initializeNative` prepares and configures an explicit engine-enabled
build. It requires the complete matching runtime tree under APK assets
`darktable-runtime/`, recursively extracts it into a private, installation
version-specific directory beneath `filesDir/darktable-runtime/`, and commits
the tree only after a successful copy. It never merges a new installation over
an old tree. A failed extraction is not reused; previously committed versions
are retained so an active native session cannot lose its files. The native data
directory is the committed tree's `data/` child; sibling resources such as
`lensfun/version_1/` retain their layout relative to that data directory.

The minimum checks require nonempty `darktablerc`, `rawspeed/cameras.xml`,
`noiseprofiles.json` and `wb_presets.json`. The first is generated by the desktop
build, and the others are read directly by the RAW decoder, denoise profiles and
white-balance initialization. These checks are necessary startup conditions;
they do not establish a complete runtime. Package the matching installation's
data, including styles and watermarks, any supplied `color/in` and `color/out`
profiles, and the data required by enabled optional features such as OpenCL
kernels or AI models. Lensfun calibration data uses a separate runtime layout
and must also be integrated before claiming lens correction parity.

The existing Gradle asset task only supplies three flat camera/noise/white-balance
database files. It does not produce this runtime tree, so an engine-enabled build
with only those files fails preparation with an explicit missing-runtime error.
`tempRoot` uses the application's cache directory. `moduleDir` is always
`applicationInfo.nativeLibraryDir`: processing and export libraries must be
packaged as Android native libraries and extracted by the installer. The helper
rejects `.so` files and ELF content in data assets; it never copies executable
code to `filesDir` or changes process environment variables. A successful native
configuration is retained for the process. A recreated editor reuses identical
paths while its previous ViewModel finishes closing a session; a changed runtime
still goes through native validation and cannot replace an active session's data.

The experimental [runtime staging tool](../mobile/dependencies/STAGING.md)
prepares this tree and its native-library closure from a complete installed
Android engine. The explicit [prebuilt engine route](PREBUILT_ENGINE.md)
consumes the verified stage when selected by a project property. Native
build/link, staging and final APK runtime-content validation have passed for the
configured bootstrap. A preserved APK pair also passed the separate ARM64/API 28
Fusion JNI edit/export test with the recorded host execution settings. Disabled
optional codecs, unported controls and broader device/format checks remain separate work.

## Restoring the Gradle Wrapper binary

The wrapper scripts and `gradle/wrapper/gradle-wrapper.properties` are intentionally kept in this repository. This change omits the newly-added `gradle-wrapper.jar` because the Codex pull-request transport used for this change cannot represent new binary files. Before building, restore the official wrapper JAR in a separate local commit from a trusted Gradle installation:

```bash
cd android
gradle wrapper --gradle-version 8.14.4 --distribution-type bin
git add -f gradle/wrapper/gradle-wrapper.jar
git commit -m "Add official Gradle wrapper JAR"
```

Review the generated changes before committing; the command should agree with the checked-in Gradle 8.14.4 wrapper properties. Do not download an arbitrary JAR or replace the wrapper with a custom binary. The wrapper JAR is the only expected binary source file; APKs, AABs, native libraries, build directories, and signing material must remain untracked.

CI and release jobs generate this ignored wrapper JAR from the pinned Gradle 8.14.4 installation before wrapper validation and every Gradle build. Therefore fork pull requests and release runners do not depend on a binary being committed to the repository.
