<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android port plan

## Phase 0 findings (2026-08-06)

The checkout is a shallow (`459` commits) but genuine darktable Git history at `249f20eda79fdbc650e89778694c4829dba64d4b`; submodule gitlinks are present but uninitialized. No tags or remotes were supplied. `upstream` was added as `https://github.com/darktable-org/darktable.git`; an origin owner therefore cannot be inferred and the temporary namespace is `org.example.darktableandroid`. The README identifies 5.6.0 as the stable release, while this commit is later development code.

### Build and coupling inventory

* Root CMake always enters `src`, data, documentation, tools, and packaging, and requires desktop-oriented gettext/introspection tools. `src/CMakeLists.txt` constructs a shared `lib_darktable`, but its source glob combines database, control/jobs, pixelpipe, GTK widgets, GUI, views, and image I/O; it is not an Android-ready engine library.
* `darktable-cli` is a thin executable linked to that same `lib_darktable`. It demonstrates headless orchestration, but does not remove GTK/global initialization or plugin coupling.
* RAW paths include RawSpeed and optional LibRaw; CR3 also needs Exiv2 with ISO-BMFF. Core requirements include GLib/GThread/GModule, SQLite, Exiv2, LCMS2, JPEG, PNG and TIFF. Each must be cross-built and license-audited for Android.
* Pixelpipe lives in `src/develop/pixelpipe.c`; export orchestration in `src/imageio/imageio.c` initializes an export pixelpipe, supplies input, creates/synchronizes nodes, and processes output. ROI/tiling support in `src/develop/tiling.c` is the correct memory foundation.
* IOPs and image formats/storage are CMake `MODULE` libraries loaded through GModule. Android can support dynamic libraries, but desktop install paths and discovery are unsuitable. The preferred mobile target will statically select modules and register their entry points through a generated registry, without broadly adding `__ANDROID__` conditionals.
* Relevant feature switches already disable OpenCL, AI, Lua, camera/tethering, map, print, GMIC, SDL, and optional formats. They do not remove GTK or split core from desktop UI.
* POSIX assumptions to audit include pthreads, signals, locale, filesystem/config paths, `mmap`, process spawning, and module loading. Adapters are needed for storage/file descriptors, cache/config directories, logging, memory pressure and cancellation. GLib threading can remain if cross-built.

### Safe extraction sequence

1. Keep desktop targets unchanged. Add an opt-in mobile target that first compiles upstream RAW loader primitives and their required dependencies.
2. Split initialization needed by CLI/export from GUI initialization, then define an explicit static registry for `rawprepare`, `demosaic`, `temperature`, `exposure`, `highlights`, `colorin`, `filmicrgb`, `colorbalancergb`, `crop`, `flip`, `ashift`, `colorout`, and `finalscale`.
3. Back a single-image record/history with an app-private SQLite database. Do not expose library collection behavior and do not write source/XMP.
4. Connect `dt_mobile_render_preview` to a 2048-pixel, cancellable tiled CPU pipe. Only after a native DNG preview is verified should JNI expose a direct buffer/bitmap-copy boundary.
5. Add JPEG/PNG exports to a cache output which Kotlin copies into MediaStore or a create-document URI.

## Phase 1 boundary and ownership

`mobile/include/dt_mobile.h` is platform-neutral and opaque. The caller owns session handles and must close them; render output will be core-owned allocation released by `dt_mobile_free_buffer`. Cancellation is thread-safe. The current compile spike validates arguments, file access, errors, and cancellation but intentionally returns `UNSUPPORTED`: it does **not** claim RAW decode or pixelpipe integration.

Kotlin owns `content://` access. It optionally persists grants, streams to a bounded app cache, hashes content for identity, and sends only a cache filesystem path to JNI. Sources are never overwritten. Future JNI can accept duplicated file descriptors without changing the processing API.

## Build/release status and blockers

The Android skeleton uses API 26 minimum, compile/target 35, NDK ABI `arm64-v8a`, CMake 3.22.1, AGP 8.9.1, Kotlin 2.1.20 and JDK 17 in CI. R8/resource shrinking is explicitly deferred until JNI/Compose keep behavior is tested. The blocking engineering task is extracting a GTK-free initialization/pixelpipe subset and obtaining reproducible Android builds of RawSpeed, Exiv2, GLib, SQLite, LCMS2 and codecs. No Android bitmap fallback is permitted.

Release signing reads environment variables only. `devRelease` uses the debug key and `.dev`; production is unsigned without credentials and the release workflow fails before signing when secrets are absent. Source archives use recursive submodule checkout and `git archive` per repository so gitlink contents are included.
