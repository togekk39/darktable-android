<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Experimental Android runtime staging

`stage-android-runtime.py` requires Python 3.10 or newer and assembles a
**previously built and installed** ARM64
Android library-only engine. It does not build missing dependencies, modify ELF
files, enable the Gradle engine flag, build an APK, or prove Android execution.
The complete Android core/GTK graph still needs to build successfully before a
full engine installation can be staged and tested.

After the experimental root engine build succeeds, install that exact build to
a fresh private prefix. Use the same build configuration throughout:

```sh
cmake --install "$DT_MOBILE_ENGINE_BUILD" --prefix "$ANDROID_ENGINE_PREFIX"
python3 mobile/dependencies/stage-android-runtime.py \
  --prefix "$ANDROID_ENGINE_PREFIX" \
  --library-root "$ANDROID_DEPENDENCY_PREFIX/lib" \
  --lensfun-data "$ANDROID_DEPENDENCY_PREFIX/share/lensfun/version_1" \
  --ndk "$ANDROID_NDK_HOME" \
  --output "$ANDROID_RUNTIME_STAGE"
```

The output directory must not already exist or lie within an input directory.
No previous staging directory is merged, deleted or overwritten. Files are
copied into a temporary sibling and the complete directory is renamed into
place after validation. The result contains:

* `assets/darktable-runtime/data/`: the installed `share/darktable/` tree.
* `assets/darktable-runtime/lensfun/version_1/`: the matching Lensfun calibration
  directory. When it is installed in the engine prefix, `--lensfun-data` can be
  omitted.
* `jniLibs/arm64-v8a/`: one flat directory containing the installed core, JNI,
  every configured processing/format/storage plugin and their shared-library
  dependencies, retaining the exact library filenames.
* `manifest.json`: original source paths, SHA-256 values, ELF SONAME/DT_NEEDED,
  system-library dependencies and data hashes for review.
* `android-runtime-libraries.txt`: the original configured module list, also
  recorded in the JSON `configuredLibraries` field, for later APK verification.

The Android engine CMake configuration generates and installs
`share/darktable-mobile/android-runtime-libraries.txt` from its actual core,
JNI and `DT_PLUGIN_IOPS/FORMATS/STORAGES` targets. Staging requires every entry
to exist in `prefix/lib/` or `prefix/lib64/`; dynamically loaded plugins cannot
be inferred from the JNI library's link dependencies alone. Missing JNI/core,
missing plugin categories, repeated manifest entries and an incomplete install
are rejected. This manifest describes the configured modules; optional codecs
disabled during bootstrap do not become supported by staging.

Every staged library must be little-endian ELF64, AArch64, ET_DYN without an
executable interpreter. All non-system DT_NEEDED dependencies must resolve to
validated Android libraries from the installation, repeatable `--library-root`
directories or the specified NDK. Differing files with the same destination
name are rejected; identical files may be shared. A SONAME must match the
staged filename. Versioned `.so.0` names are rejected because the Android JNI
source set must package the exact `.so` filenames expected by the linker. Fix
the dependency build instead of renaming a library or editing its binary here.
ELF RPATH/RUNPATH may be empty or contain only `$ORIGIN`/`${ORIGIN}`; absolute
paths, working-directory searches and parent/subdirectory paths are rejected.
The Android-only JNI and processing/format/storage install rules now select
`$ORIGIN`, matching the core and the flat layout. Stale builds or dependencies
that retain desktop paths such as `$ORIGIN/../../..` must be relinked. This tool
never rewrites a search path.

Only `libc.so`, `libm.so`, `libdl.so`, `liblog.so`, `libandroid.so` and `libz.so`
are treated as Android API 26 system libraries. With `--ndk`, the tool also
checks their AArch64 API 26 stubs. Other libraries must be packaged; specifically,
`libc++_shared.so` and `libomp.so` are resolved from the NDK and copied when
needed. Linux names such as `libc.so.6`, X11 libraries and host executables are
not allowed to pass as Android system dependencies. Native symbol resolution,
Android API use, platform library namespaces and loading still need the actual
NDK link checks and device execution; a DT_NEEDED closure is not that proof.

Runtime assets must contain the nonempty generated `darktablerc`,
`rawspeed/cameras.xml`, `noiseprofiles.json` and `wb_presets.json`, as well as
nonempty Lensfun calibration XML. All other installed darktable data is retained.
ELF content, `.so` names and symlinks are rejected in data assets. Executable
code stays in Android's extracted `nativeLibraryDir`, matching `RuntimeAssets`.
No files from `prefix/bin/` are copied. Four required data files and an XML file
are necessary checks, not proof of complete optional runtime data.

For an isolated audit of an existing real Android probe and its shared closure,
without pretending it is a complete engine installation:

```sh
python3 mobile/dependencies/stage-android-runtime.py \
  --inspect-library path/to/libactual_android_probe.so \
  --library-root path/to/other/android/libraries \
  --ndk "$ANDROID_NDK_HOME"
```

This mode prints a JSON report and does not create assets or a staging directory.
The actual LibRaw parity and Lensfun ARM64/API 26 probes can be audited this way.
The optional [prebuilt Gradle route](../../android/PREBUILT_ENGINE.md) consumes
these verified sources without configuring a second dependency-only JNI build.
Full engine staging, installer extraction, module discovery and device RAW/GPR
edit/export validation remain pending until the whole native engine graph is
available. Third-party license/source distribution obligations
also remain with the final packaged build; this tool copies runtime files only.
