<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android native engine execution test

`NativeEngineSmokeTest` runs the installed Android JNI/core/modules through the
same public adapter used by the editor. It requires an ARM64 Android API 26+
device or an actual ARM64 Android emulator, a complete engine-enabled APK and an
original RAW/GPR fixture. The default dependency-only APK fails its capability
check. Building the test APK alone is not Android execution evidence.

Build both APKs from a complete verified stage, using the tools described in
[PREBUILT_ENGINE.md](PREBUILT_ENGINE.md):

```sh
android/gradlew --no-daemon -p android \
  :app:assembleDevDebug :app:assembleDevDebugAndroidTest \
  -PdarktableRuntimeStage=/absolute/path/to/real-runtime-stage
```

The test APK is
`android/app/build/outputs/apk/androidTest/dev/debug/app-dev-debug-androidTest.apk`.
It contains no RAW sample or native processing replacement. In the managed
development environment, the local Gradle init script instead redirects both
APKs to `work/android-build/_app/outputs/apk/` and supplies the private work
directory's debug signing key.

Copy a fixture into the target application's private files directory. This does
not require image/storage permissions, a document-picker mock or a gallery UI.
For example, `GPBK7066.GPR` from the pinned official GoPro archive has the
previously verified output dimensions `3104 x 3000`:

```sh
ADB_BIN="$ANDROID_SDK_ROOT/platform-tools/adb"
TEST_SERIAL=emulator-5554
APP_PACKAGE=org.example.darktableandroid.dev

"$ADB_BIN" -s "$TEST_SERIAL" install -r /path/to/app-dev-debug.apk
"$ADB_BIN" -s "$TEST_SERIAL" install -r /path/to/app-dev-debug-androidTest.apk
"$ADB_BIN" -s "$TEST_SERIAL" shell run-as "$APP_PACKAGE" mkdir -p files/engine-smoke
"$ADB_BIN" -s "$TEST_SERIAL" exec-in \
  "run-as $APP_PACKAGE sh -c 'cat > files/engine-smoke/input.GPR'" \
  < /path/to/original/GPBK7066.GPR
APP_DATA_DIR=$("$ADB_BIN" -s "$TEST_SERIAL" shell run-as "$APP_PACKAGE" pwd | tr -d '\r')

"$ADB_BIN" -s "$TEST_SERIAL" shell am instrument -w -r \
  -e class org.example.darktableandroid.nativecore.NativeEngineSmokeTest \
  -e inputFile "$APP_DATA_DIR/files/engine-smoke/input.GPR" \
  -e expectedWidth 3104 -e expectedHeight 3000 \
  -e timeout_msec 3600000 \
  "$APP_PACKAGE.test/androidx.test.runner.AndroidJUnitRunner"

"$ADB_BIN" -s "$TEST_SERIAL" exec-out run-as "$APP_PACKAGE" \
  cat files/engine-smoke/result.json > android-engine-result.json
python3 -c 'import json; r=json.load(open("android-engine-result.json")); assert r["status"] == "passed", r'
```

Use the device's actual serial and matching debug signatures. Fixture files,
APKs, keys, exports and result logs are generated inputs/outputs and must remain
outside source control. The fixture path must resolve inside the target app's
private files directory. No original input is overwritten.

The single execution test checks the real engine and VC-5 capability bits,
runtime extraction/configuration, original RAW open, actual preview pixels,
exposure changes, Kelvin/tint through the shared desktop calculation core,
pixel-exact undo/redo, native full-size PNG export and Android PNG decoding,
source SHA-256 protection, and close/reopen with the original pixels restored.
The PNG check establishes full dimensions and decodability; it does not compare
that export against a second unedited full-resolution export. Preview edit
changes are compared separately.

`files/engine-smoke/result.json` records the device/API/ABI, current phase,
completed phase durations, preview hashes, original-source hashes and completed
export location/hash. The file is updated before each native operation; a
native crash can leave `status: running` at its last phase. A Java/JNI exception
records `status: failed`. Require both the instrumentation runner's successful
test result and `status: passed`; a shell command's exit status alone is not
sufficient. Successful exports remain under a unique private `run-*` directory
for inspection. The tag `NativeEngineSmoke` in logcat provides live phase markers.

Even a successful run covers one fixture on that Android runtime. It does not
establish all RAW/camera types, hardware/driver behavior, every desktop editing
tool, all optional codecs, UI interaction, lifecycle interruption or complete
desktop feature parity.

## Optional ARM64 emulator instruction diagnostic

The standalone sources in
[`mobile/platform/android/tests`](../mobile/platform/android/tests) reproduce an
ARM64 vector instruction sequence independently of darktable. They are not
linked into the APK or engine and are not RAW processing tests. The assembly
preserves the contiguous sequence from an NDK 27.2 vectorized `size_t` loop; the
C checker is compiled at `-O0`, checks allocation canaries and verifies all
65,536 resulting values. The executable uses neither OpenMP nor libm.

Build it with the pinned NDK into an explicitly supplied fresh directory, then
run it on the Android runtime being investigated:

```sh
PROBE_BUILD="$PWD/work/vector-sequence-probe"
mobile/platform/android/tests/build-vector-sequence.sh \
  --ndk "$ANDROID_NDK_HOME" --output "$PROBE_BUILD"
"$ADB_BIN" -s "$TEST_SERIAL" push "$PROBE_BUILD/vector-sequence" \
  /data/local/tmp/dt-vector-sequence
"$ADB_BIN" -s "$TEST_SERIAL" shell chmod 700 /data/local/tmp/dt-vector-sequence
"$ADB_BIN" -s "$TEST_SERIAL" shell /data/local/tmp/dt-vector-sequence \
  > vector-sequence-result.txt
cat vector-sequence-result.txt
```

Require exit status zero and the final `all65536values=exact` marker. Capture
logcat/tombstone evidence on a native crash. Generated binaries and logs stay
outside source control; the build script rejects an existing output directory
and checks the ELF's shared-library dependencies.

On 2026-10-26, the original standalone assembly reproducer failed with
`SIGSEGV` on official emulator 30.8.4 build 7600983, running the unmodified
official ARM64 API 26 image. The same investigation found that the original
vectorized OpenMP loops failed, while nonvectorized diagnostic loops passed;
isolated `powf` ABI preservation and DUP instruction probes also passed. The
standalone sequence removes OpenMP, libm and image processing from that failure
case. Verification on emulator 31.3.14 is pending. These observations do not
justify changing desktop algorithms or disabling production vectorization.
