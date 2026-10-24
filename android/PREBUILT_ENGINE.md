<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Optional prebuilt Android engine

The default Android build still compiles the dependency-only native bridge with
the processing engine disabled. A complete native Android engine can instead be
supplied explicitly as a verified prebuilt stage. This does not make an
incomplete dependency probe into an editor or establish device support.

First finish the ARM64/API 26 [engine build](../mobile/dependencies/ENGINE_BOOTSTRAP.md),
install it, and run the current [staging tool](../mobile/dependencies/STAGING.md)
with `--ndk`. Use one matching engine version/configuration for the core, JNI,
plugins and complete runtime data. The resulting stage must include:

```text
manifest.json
android-runtime-libraries.txt
assets/darktable-runtime/data/...
assets/darktable-runtime/lensfun/version_1/...
jniLibs/arm64-v8a/*.so
```

Build using its absolute path:

```sh
./gradlew :app:assembleDevDebug \
  -PdarktableRuntimeStage=/absolute/path/to/android-runtime-stage
```

This branch disables the standalone CMake build and its dependency-preparation
task. It uses only the stage's native libraries and complete runtime assets,
avoiding a collision with the dependency-only `libdt_mobile.so`. The ABI remains
`arm64-v8a`. Existing installer extraction into `nativeLibraryDir` remains
enabled for native plugin discovery.

Each variant registers one generated-source producer for both assets and JNI
libraries. Python 3.10+ verifies the input stage and a fresh copied generation
under `app/build/generated/prebuiltRuntime/<variant>/`. The task then copies
those verified sources to AGP's generated assets/JNI directories and verifies
the actual files AGP will package against the same manifest:

* SHA-256 values and exact native/data file lists, including no unrecorded files.
* The original configured module list against `configuredLibraries` in JSON and
  every packaged core/JNI/processing/format/storage module.
* AArch64 ELF64 libraries, SONAME/search paths and the complete non-system
  `DT_NEEDED` closure. JNI must depend on `libdarktable.so`; a dependency-only
  JNI probe is rejected.
* API 26 system-stub verification recorded by the staging producer, required
  nonempty runtime data and Lensfun calibration XML.
* Native code only in the flat JNI directory, with no ELF or `.so` files in data
  assets, symlinks, extra ABIs or unexpected asset roots.

The original configured list normally travels in the stage. If it is stored
separately, provide its absolute path with
`-PdarktableRuntimeModules=/absolute/path/android-runtime-libraries.txt`. It must
exactly match the JSON list; it does not repair an old or incomplete stage.
Use `-PdarktablePython=/absolute/path/to/python3` when `python3` is not on PATH.
The verifier can also run independently:

```sh
python3 ../mobile/dependencies/verify-android-runtime-stage.py \
  --stage /absolute/path/to/android-runtime-stage
```

Direct asset/native merge tasks use the same verified producer, rather than
copying unchecked source directories. Generated files are build outputs; do not
edit them or place them in source control. Remove the property to return to the
default dependency-only route. A missing/incomplete stage fails explicitly and
does not fall back to an engine-disabled APK.

There is not yet a complete engine stage validated on Android. Full core/module
linking, APK contents, native loader behavior and RAW/GPR edit/export/cancel on
real devices still require verification. The configured manifest covers only
the selected native build; optional codecs and unported editing controls remain
separate work.
