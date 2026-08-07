<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android build

Open this `android/` directory in Android Studio. Install Android SDK 35, NDK 27.2.12479018, and CMake 3.22.1. Build `devDebug` or run `./gradlew :app:assembleDevDebug`; install with `adb install -r app/build/outputs/apk/dev/debug/app-dev-debug.apk`.

The current Phase 1 build is an honest UI/native boundary skeleton. It accepts document URIs but does not yet render RAW data.

## Restoring the Gradle Wrapper binary

The wrapper scripts and `gradle/wrapper/gradle-wrapper.properties` are intentionally kept in this repository. This change omits the newly-added `gradle-wrapper.jar` because the Codex pull-request transport used for this change cannot represent new binary files. Before building, restore the official wrapper JAR in a separate local commit from a trusted Gradle installation:

```bash
cd android
gradle wrapper --gradle-version 8.14.4 --distribution-type bin
git add -f gradle/wrapper/gradle-wrapper.jar
git commit -m "Add official Gradle wrapper JAR"
```

Review the generated changes before committing; the command should agree with the checked-in Gradle 8.14.4 wrapper properties. Do not download an arbitrary JAR or replace the wrapper with a custom binary. The wrapper JAR is the only expected binary source file; APKs, AABs, native libraries, build directories, and signing material must remain untracked.
