# Android native dependencies

The Android engine uses the `arm64-android` triplet at API 26 and the repository's
NDK 27.2.12479018. `build-android-dependencies.sh` checks out the immutable vcpkg
2025.06.13 release catalog, whose portfiles pin upstream source versions and
SHA-512-check every downloaded archive, then builds from source. The checked-in
overlay triplet sets `VCPKG_CMAKE_SYSTEM_VERSION` so all dependencies are
compiled for the advertised API 26 baseline. It also supplies the canonical
`aarch64-linux-android` host tuple used by Autoconf-based ports, preventing
vcpkg's make helper from falling back to an ARMv7 compiler for ARM64 builds.
The `ports/libiconv` overlay supplies the explicit `--host` option required by
the older `vcpkg_configure_make` helper used by the pinned libiconv port. This
makes Autoconf enter cross-compilation mode instead of executing Android probe
binaries on the build host; other make-based ports retain the canonical tuple.
Nothing from
`.vcpkg/` is checked in. Gradle runs this same command before CMake and CI caches
only its installed output and download cache.

| Dependency | Pinned source | Link | License | Upstream reason |
|---|---|---|---|---|
| RawSpeed | repository gitlink `7cf3dc3b9d9c82b414198b1f57460478be6c6c9d` | static | LGPL-2.1-or-later | primary RAW parser/decoder and camera metadata |
| Exiv2 | vcpkg 2025.06.13 `exiv2` port | static | GPL-2.0-or-later | EXIF/XMP and ISO-BMFF/CR3 metadata |
| GLib | vcpkg 2025.06.13 `glib` port | static | LGPL-2.1-or-later | upstream core containers, threads, I/O and module abstractions |
| SQLite | vcpkg 2025.06.13 `sqlite3` port | static | public domain | darktable image/history database |
| Little CMS 2 | vcpkg 2025.06.13 `lcms` port | static | MIT | input/output color transforms |
| zlib | vcpkg 2025.06.13 `zlib` port | static | Zlib | compressed metadata and codec dependency |
| libjpeg-turbo | vcpkg 2025.06.13 `libjpeg-turbo` port | static | BSD-3-Clause, IJG, Zlib | JPEG input and export |
| libpng | vcpkg 2025.06.13 `libpng` port | static | libpng-2.0 | PNG input and export |
| libtiff | vcpkg 2025.06.13 `tiff` port | static | libtiff | TIFF/DNG containers and TIFF export |
| pugixml | vcpkg 2025.06.13 `pugixml` port | static | MIT | RawSpeed camera XML database |
| LibRaw | vcpkg 2025.06.13 `libraw` port | static | LGPL-2.1-only or CDDL-1.0 | upstream fallback for files RawSpeed cannot decode |

The catalog tag is the version pin for every vcpkg port; the precise upstream
version, port revision, source URL and archive SHA-512 are recorded in
`ports/<name>/vcpkg.json` and `portfile.cmake` in that tag. Transitive packages
(notably PCRE2 and libffi for GLib, and codec dependencies selected by the
ports) are locked and checksum-verified by the same catalog. Android's Bionic,
`liblog`, C++ shared runtime, pthread and dynamic loader come from the pinned
NDK rather than being duplicated.

All third-party libraries above are linked into `libdt_mobile.so` statically;
only the NDK's `libc++_shared.so` is packaged as another runtime library. Static
linking is deliberate: it avoids unstable plugin lookup paths and ensures the
APK carries a single auditable native engine. LGPL replacement/relinking and
GPL corresponding-source obligations apply to distributed APKs; the production
workflow's recursive source archive and the notices at repository root must be
distributed with releases.

RawSpeed's `cameras.xml`, plus darktable's `noiseprofiles.json` and
`wb_presets.json`, are copied to generated Android assets. Only those runtime
databases are packaged—not the desktop data tree. Gradle's `prepareMobileAssets`
task makes the copy deterministic for every variant.

## Direct build and verification

```sh
export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
mobile/dependencies/build-android-dependencies.sh
./android/gradlew -p android testDevDebugUnitTest assembleDevRelease
unzip -l android/app/build/outputs/apk/dev/release/app-dev-release.apk
```

Host API tests intentionally do not require this Android-only bundle. The CI
verification additionally uses `llvm-readelf -d` on the unpacked library and
checks that the three databases and `libc++_shared.so` are present.
