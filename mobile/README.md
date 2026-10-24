<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Mobile engine integration

The target is the existing Android single-image interface with the desktop
darktable processing algorithms, RAW inputs (including GoPro GPR), and desktop
export capabilities. A lighttable, gallery scan, and image browser are outside
this interface. Darktable's internal image/history database is still required
by its processing core; it does not imply a browser screen.

## Implemented boundary

`dt_mobile.h` exposes module descriptions, numeric parameter changes, dedicated
curve edits, atomic white-balance gains/settings, freehand cropping, module enable/disable, session undo/redo,
preview, installed export formats and full-resolution export settings.
The engine adapter uses upstream introspection structures, develop history and
the imageio pixelpipe. It does not duplicate editing formulas. Parameter bounds,
integer types and enum choices are checked before changes are committed. Numeric
controls also require an explicit review in `core/dt_mobile_parameter_policy.h`:
introspection bounds alone do not express curve, array or other cross-field
invariants. Unknown fields are neither advertised nor writable by the numeric API.
The reviewed exposure controls additionally reject combinations where the black
point reaches or exceeds the exposure-derived white point, matching the relation
maintained by the desktop widget without silently changing another control.

JSON module descriptions contain `operation`, `name`, `enabled`, `canDisable`,
`parameters`, `curves` and optional `crop`/`rgbCurveOptions`/`whiteBalance` descriptions. Parameters contain `name`, `label`, `type`, `value`, `min`,
`max` and optional `choices` (`value`, `label`). The current reviewed scalar subset
includes exposure/black correction and compensation switches, Velvia strength/bias,
Lab color contrast, six global color-balance RGB grading controls, Sigmoid
contrast/skew/hue preservation, and sharpen amount/threshold (19 controls).
Float inputs are checked in their native float representation so decimal values
at a boundary, such as `0.1`, remain usable. Non-finite and overflowing values
are rejected. Nested fields,
arrays, curve counts and other unreviewed parameters need purpose-built APIs that
preserve upstream invariants. The complete module list and enable/disable controls
remain available, even when a module has no reviewed numeric controls. Descriptions are
allocated for the caller, who must release them with `dt_mobile_free_buffer`.

`dt_mobile_set_curve` updates a complete node list and its interpolation in one
history change for basecurve and RGB curve. The editor supports 2–20 finite
nodes in normalized coordinates, with at least `1e-6f` horizontal separation,
and the upstream cubic spline, Catmull-Rom and monotone Hermite interpolation
methods. Metadata supplies each supported curve's channel, nodes, interpolation,
limits and samples calculated by the upstream spline routines. Unsupported
historical curve states are omitted from this metadata without altering history.
Basecurve exposes channel 0. RGB curve exposes its combined channel in linked
mode, or its three channels in independent mode, according to the existing
module state. Editing preserves the other channels and unrelated parameters.
`dt_mobile_set_rgb_curve_options` atomically changes linked/independent mode and
one of the seven original preserve-color methods. The Android editor displays
the choices advertised by the native module, with preserve-color controls in
linked mode. All three stored channels are validated before the batch can enable
the module. When switching from linked to independent, the desktop rule copies
the shared curve into G and B only if both remain identity curves; otherwise
both are preserved. One undo restores the mode, method and any copied channels.
Middle-gray compensation and other advanced curve settings remain pending.
Internal node counts stay excluded from the generic numeric API.

`dt_mobile_set_whitebalance_gains` applies all active coefficients together,
selects the original user-defined setting and clears its late color-adaptation
mode in one history step. The reviewed manual range is `0.001f..8.0f`; this is
narrower than the desktop slider's zero-inclusive range. Inputs are never
silently clamped or green-normalized. `dt_mobile_set_whitebalance_setting`
selects the image's original as-shot (0), camera-reference (3), or as-shot-to-
reference (4) coefficients and processing mode together. Available settings
come from the current develop's original coefficient sources; valid named
coefficients above 8 remain intact and visible.

White-balance metadata supplies active channel indexes/labels/current values,
manual bounds, current setting and available named settings. Three-channel
edits preserve the inactive fourth coefficient, including its NaN sentinel.
Four-Bayer channel names and CYGM display order follow the desktop; the JNI
array remains in native index order. True camera-monochrome inputs omit/reject
the editor. Non-RAW inputs retain the desktop's optional manual processing.
Snapshots retain chroma state, and restore synchronizes it with the actual
temperature module before subsequent operations. Kelvin/tint, spot sampling
and camera-database preset selection still need dedicated support.

`dt_mobile_set_crop` applies a complete normalized left/top/right/bottom
rectangle through the desktop crop module in one history change. It validates
the upstream field layout and a minimum extent of `0.01f`, comparing minimum
edges using the desktop widget's float addition. The batch also selects the
desktop freehand aspect setting, so an earlier fixed ratio cannot change the
requested rectangle during export. Crop metadata contains the four edges,
`minSize` and whether the current parameters already use freehand proportions.
These coordinates belong to the crop module's input space. The Android editor
uses numeric coordinates and a separate rectangle diagram; it does not treat
the already cropped preview as the original image. Fixed aspect presets and a
touch overlay on an uncropped image remain to be implemented.

`dt_mobile_history`, `dt_mobile_undo` and `dt_mobile_redo` expose the current
session's edit window. Each successful Apply creates a separate snapshot, even
when desktop history merges consecutive changes to one module. The window holds
up to 64 steps; editing after undo discards redo. Snapshots use the original
history, module-order and mask-copy routines. Undo restores the original
parameters and database history used by the preview/export pixelpipes. These
steps do not persist after closing the image and do not expose automatic startup
history as user edits. The Android toolbar disables unavailable actions and
serializes history changes with editing and rendering.

`dt_mobile_set_module_params` is a low-level interface for trusted native
integrations that already own a validated upstream parameter struct for the exact
engine version. It checks the byte size, not cross-field validity, and must not
receive arbitrary input or UI-provided blobs. Android does not expose it through JNI.

The Android editor reads these descriptions and calls the JNI adapter. Export
encodes to a private cache file at native resolution before copying the completed
file to a document selected by the user. Preview bitmaps are not export inputs.
Native exports use a temporary destination and rename on success; the public API
rejects destinations that refer to the original input. Session operations are
serialized by the owner; only cancellation can overlap them, and close waits
for the owning job to finish.

`dt_mobile_export_with_settings` adds aspect-preserving maximum dimensions
(zero means unrestricted, without upscaling), PNG 8/16-bit unsigned output,
TIFF 8/16-bit unsigned or 32-bit float output, format-specific compression,
JPEG quality/subsampling, JPEG/TIFF DPI, available built-in output profiles,
rendering intent and a source-metadata policy. Format metadata advertises the
supported controls. The original format plugins and color-management pixelpipe
perform the encoding. Temporary format configuration remains active through the
writer and is restored on success, failure and cancellation. The legacy export
API retains its quality range, including the desktop JPEG minimum-quality clamp.
“Do not copy source metadata” suppresses source EXIF/XMP copying; required encoder
fields and ICC profiles can remain. Custom ICC files, half-float output, layers
and the remaining codec-specific options still require dedicated support.

`dt_mobile_configure_runtime(data_dir, module_dir, temp_root)` copies absolute
paths before the first image is opened. It rejects incomplete startup data,
unwritable temporary storage, and changes while a session or runtime init/cleanup
is active. The configuration survives closing the last session. Config, cache
and pixelpipe temporary files stay inside a fresh private child of `temp_root`;
the child is removed when the runtime closes. Host tools may instead keep using
`DT_MOBILE_DATADIR` and `DT_MOBILE_MODULEDIR` without configuration. The API never
changes process environment variables. Android prepares its versioned data tree
and configures `applicationInfo.nativeLibraryDir` through this entry point.

`dt_mobile_capabilities()` reports the processing engine only when it is linked.
A dependency-only build rejects open with `DT_MOBILE_ERROR_UNSUPPORTED`, rather
than returning a successful session that can never process an image.

## GoPro GPR

The pinned desktop RawSpeed includes a VC-5 decoder in its DNG processing path.
The mobile desktop-engine build reads the original GPR directly through this
same decoder, and GPR is classified as RAW rather than LDR. GPR support does not
require a second production decoder or an embedded JPEG preview.

`DT_MOBILE_WITH_GPR_SDK=ON` additionally builds the official GoPro SDK as an optional
independent reference decoder and supplies its six real verification samples.
It uses immutable commit `446c736a38fb14f51343605c0780d347dc602f89`, with the archive
SHA-256 verified in `cmake/GprSdk.cmake`. Its isolated helper can decode GPR into
a private DNG cache, but the desktop-backed mobile build continues to use
RawSpeed directly. The helper never changes the original source, and removes
temporary files on failure or cleanup.

The GPR capability requires a linked processing engine with its VC-5 decoder.
Building only the reference SDK does not make the Android application able to
edit RAW files. The optional SDK contains Adobe DNG/XMP and other third-party
code: its corresponding licenses and source must accompany any distributed
build that includes it.

## Verification

For an opt-in processing-library build from the repository root, set
`BUILD_MOBILE_LIBRARY_ONLY=ON`. This also enables the mobile engine and keeps
`lib_darktable`, all configured processing and format/storage modules, and runtime data,
while excluding desktop views, lighttable plugins, executables, documentation
and packaging. Build `dt_mobile_runtime` to produce the complete dynamic module
set. This mode is independent of Gradle's current engine-disabled dependency
probe and does not establish Android build, packaging or device support.

Dependency-free public API and adapter-boundary tests:

```sh
cmake -S mobile -B build/mobile -DDT_MOBILE_BUILD_TESTS=ON \
  -DDT_MOBILE_WITH_DARKTABLE_ENGINE=OFF
cmake --build build/mobile
ctest --test-dir build/mobile --output-on-failure
```

These tests cover unavailable-engine reporting, argument validation, ownership,
error propagation, cancellation and source-file protection. The mock adapter
test does not establish RAW decode support.

To test the actual VC-5 decoder independently, add `-DDT_MOBILE_WITH_GPR_SDK=ON` to
the same configure command. The SDK archive includes six real official samples:
HERO5, HERO6, HERO7, HERO9 and both Fusion lenses. Each test verifies full image
dimensions, every decoded Bayer sensor byte through a GPR-to-DNG round trip,
unchanged source data and private cache cleanup.

With the desktop engine also enabled, `dt_mobile_raw_engine_test` opens each
original GPR through the public API and RawSpeed, changes exposure, exports a
full-size PNG, decodes that PNG, and confirms the source GPR remains unchanged.
`dt_mobile_raw_whitebalance_GPBK7066` additionally uses the original Fusion GPR
to exercise native white-balance settings/gains, pixel-exact undo/redo and an
edited full-size PNG round trip. Its CLI equivalent adds `--whitebalance` to
the existing raw engine test; the other six sample tests keep their original
behavior. These remain host tests, not Android execution proof.

The real desktop-backed adapter must be configured from the repository root
with `BUILD_MOBILE_ENGINE=ON`, `DT_MOBILE_BUILD_TESTS=ON` and the normal desktop
build dependencies/submodules. Build target `dt_mobile_engine_test`, then run
`ctest --test-dir <build-directory> -R '^dt_mobile_engine_test$'
--output-on-failure`. The test changes exposure and curve nodes using upstream
processing, checks that rendered pixels change, rejects invalid curve batches
without changing module state, and verifies exported dimensions exceed the
preview and match the input. It also checks pixel-exact session undo/redo,
crop undo dimensions, the 64-step window, real PNG/TIFF precision and
compression tags, non-square bounded exports without upscaling, embedded ICC
selection, JPEG subsampling/DPI, original-source protection and exact temporary
configuration restoration, including command-line override conflicts.
White-balance checks cover real gain-dependent pixels, exact undo, named
setting semantics and inactive-fourth-channel preservation.
Cancellation after the real PNG encoder has written its temporary file also
checks that the destination stays absent, the temporary file is removed and
the original raw configuration is restored.
The `desktop-engine-adapter` job in
`.github/workflows/android-ci.yml` records the complete dependency/configure
commands. This is a desktop host test, not Android device validation.

## Work still required for the complete target

* Port the desktop core and dynamically loaded processing/format modules into
  the Android build. Core and processing headers still depend on GTK, and iop
  source files contain GUI and processing functions together. Setting the engine
  flag alone cannot resolve this.
* Package and initialize all required runtime data, color profiles, module
  registries, presets and optional codecs. The existing three Android database
  assets are insufficient for the complete desktop runtime.
* Add purpose-built touch controls and APIs for masks, white-balance Kelvin/tint,
  spot sampling and camera presets, other curve modules,
  RGB curve middle-gray compensation, fixed crop proportions, an
  uncropped touch overlay, other geometry operations, blending,
  persistent editing history, presets, multiple module instances and other compound or opaque
  parameters. Generic numeric fields, the two node editors and freehand crop are not full
  feature parity.
* Complete the export settings beyond the current dimensions, PNG/TIFF precision,
  compression, JPEG subsampling, DPI, built-in ICC profiles/rendering intent and
  source-metadata policy: custom ICC files, layers and other codec-specific
  options remain pending.
* Establish a camera/format sample matrix matching the desktop build, including
  optional RAW decoder fallbacks, and compare identical histories/exports.
  Validate Android ARM64 builds and actual devices for memory, interruption,
  lifecycle and complete RAW-to-edit-to-export behavior.

Until these are finished, the repository must not describe its APK as supporting
all desktop RAW types, all desktop tools, or standalone GPR editing.
