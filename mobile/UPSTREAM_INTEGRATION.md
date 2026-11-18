# Upstream integration

The branch is based on official darktable master
`1c4eaea9df6dbe8336449ca297acbb0120d8e968` (November 17, 2026). The root
build and every tracked upstream file remain official upstream content.
`work` retains the previous Android port history. No merge into master is
needed to build or review this branch.

The Android engine configuration lives in `mobile/cmake/EngineBuild.cmake`,
`EngineSources.cmake` and `EngineTargets.cmake`. Configure it with:

```sh
python3 mobile/tools/configure-engine.py -B build/android-engine -- \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DBUILD_MOBILE_LIBRARY_ONLY=ON
```

The complete pinned dependency/bootstrap configuration is supplied by
`mobile/dependencies/build-android-engine.sh`; use that for production engine
builds. Host integration tests use the same helper with
`-DDT_MOBILE_HOST_INTEGRATION_TESTS=ON`. Ordinary desktop builds continue to use
`cmake -S . -B build/desktop` and never compile or apply the mobile integration.

`--source-root /path/to/darktable` can point at a separate official checkout.
That checkout needs no `mobile/` files: the helper supplies this integration
directory to the generated build view. Its desktop configuration remains
independent of the Android integration repository.
The pinned Android bootstrap accepts the same source separation through
`DT_MOBILE_UPSTREAM_SOURCE=/path/to/darktable`.

`assemble-source.py` verifies `upstream-lock.json`, copies the official source
inventory into the build's `_upstream/` directory, and applies the small unified
patches in `mobile/patches/`. It accepts exact hunk positions and contexts only.
It rejects changed internal sources, headers, module metadata, submodule
revisions or tracked submodule contents. Unreviewed upstream source additions
also fail. Configuration regeneration repeats assembly before compiling.
No complete patched upstream source file is tracked in the integration layer.

Independent adapters provide Android signal handling, sidecar discovery and
module classification. Small include fragments access private module types and
helpers in the current upstream translation unit. Their patches supply only
the required internal hooks. Preview and export reuse upstream's current
pixelpipe and conversion functions. White-balance and zone-map helpers are
generated from reviewed upstream routines, so numerical implementations are
not copied into mobile sources. The former scalepixels override was removed
because current upstream provides the geometry fix.

AgX reads exposure ranges from the selected instance's original effective
exposure calculation. Automatic deflicker stores its computed correction in
the pixelpipe/GUI, so its range-read action is unavailable in a headless
editor rather than treating upstream's unavailable-value sentinel as a real
zero-EV correction. Deflicker processing itself keeps the upstream implementation.

Legacy mask editing is scoped to the editor's initial history load. Preview
and export develops keep upstream's mask migration and rendering. Existing
native flexi trees retain their processing; legacy flat-mask controls are
unavailable when they cannot represent those trees.

Android plugin names and runtime manifests are derived from the configured
upstream targets, including new modules. Android-specific source selection
occurs only in the dedicated configuration; the host integration option is
explicitly for regression tests. Platform stubs are selected only on Android.

To integrate another upstream update:

1. Fetch official `darktable-org/darktable` master and create a review branch.
   Keep the integration directories separate when updating official files.
2. Compare the new revision with the commit recorded in `upstream-lock.json`.
   Review changed internal APIs, parameter layouts, processing routines and
   every affected control policy. Remove patches superseded by upstream fixes.
3. Generate a candidate lock without overwriting the reviewed lock:

   ```sh
   python3 mobile/tools/review-upstream.py --revision upstream/master \
     --output work/upstream-lock.candidate.json
   ```

4. Adapt the small hooks to the new APIs, then explicitly replace the lock
   after review. A hash refresh alone does not establish compatibility.
5. Build the original desktop graph and the dedicated Android graph. Run
   mobile API, real engine edit/export, RAW/GPR, numerical, introspection,
   source-assembly and runtime staging tests. Run device instrumentation for
   runtime claims. Keep validation evidence tied to the exact source revision.

The lock intentionally requires review for changes to upstream internal code,
even when an existing patch still applies. This conservative boundary prevents
field-name or parameter-semantic changes from silently breaking mobile controls.
There is no build option that disables compatibility checking.

## Validation

The local integration was verified against the upstream revision above:

| Configuration | Result |
| --- | --- |
| Original desktop Release | Full build passed; 38 tests passed and 2 skipped for an optional Spektrafilm pack and unavailable GTK display. Official `0001-exposure` CPU pixel regression passed, maximum delta E 0.85975. |
| Mobile host Release, OpenCL off | All 27 tests passed, including six real GPR edit/full-resolution export cases, white-balance pixels, controls, masks, history and source protection. |
| Mobile host Debug, OpenCL on | All 27 cases passed across the decoder/export suite and complete engine control regression. CPU fallback was exercised; no OpenCL device execution was performed. |
| Android ARM64/API 26, NDK 27.2.12479018 | Full engine, JNI and configured plugin build passed. Stripped installation and stage verification passed for 115 libraries and 830 assets. |
| Engine APK and instrumentation APK | Both built and signatures verified. Final APK retained exact staged bytes; all 116 packaged native libraries passed 16 KiB alignment and ZIP alignment checks. |
| Additional checks | 148 JVM tests, 14 source-assembly checks and 37 offline runtime/staging tests passed. App lint reported no errors. |
| Independent official checkout | Configuration and compatibility-check build target passed without placing `mobile/` in the official checkout; original tracked files remained unchanged. |

Eight Robolectric classes could not run because the environment denied their
official Maven mirror downloads with HTTP 403. No test exclusions or mirror
workarounds were added to the project. Android device installation and native
execution remain unverified; APK packaging checks do not establish device behavior.
