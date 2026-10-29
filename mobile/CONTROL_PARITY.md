<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Mobile editing control coverage

The target is every desktop editing control in the existing single-image editor.
This target is **incomplete**. A module appearing in the list or offering an
enable switch does not establish that its settings have been ported.

The reviewed numeric bridge currently exposes 103 fields across 21 modules.
Continuous values have touch sliders and precise numeric input; enum parameters
have native choice menus, and booleans have switches. A completed numeric drag
submits one native change, so intermediate touch movements do not start pixelpipe
renders or create history entries. Reset buttons use the original image-specific
module defaults advertised by the engine. White-balance and crop sliders edit a
draft; their Apply buttons submit the complete dependent values together.

The original pixel algorithms remain in the desktop modules. Slider position
mapping is presentation only. Native code checks the storage type, reviewed
field identity, numeric bounds, exact enum membership and relevant cross-field
conditions before modifying parameters or history. Desktop soft slider ranges
are optional metadata; precise input retains the full reviewed hard range.

## Reviewed numeric fields

The table lists fields exposed through the generic numeric API, not a claim
that every desktop widget in these modules is implemented. Exact identities and
source review notes are in [the parameter policy](core/dt_mobile_parameter_policy.h).

| Original module | Fields exposed | Count |
| --- | --- | ---: |
| `exposure` | exposure, black, exposure-bias and highlight-preservation compensation | 4 |
| `velvia` | strength, bias | 2 |
| `colorcontrast` | Lab a/b steepness | 2 |
| `colorbalancergb` | vibrance, contrast, global chroma/saturation/brilliance, hue rotation; global offset/shadows lift/highlights gain Y/C/H; shadows/highlights weights; white fulcrum; shadows/mid-tones/highlights chroma, saturation and brilliance; saturation formula | 28 |
| `sigmoid` | middle-grey contrast, contrast skewness, hue preservation | 3 |
| `sharpen` | amount, threshold | 2 |
| `colisa` | contrast, brightness, saturation | 3 |
| `colorize` | hue, saturation, source-lightness mix, lightness | 4 |
| `tonemap` | contrast compression, spatial extent | 2 |
| `globaltonemap` | operator, Drago bias/target, detail | 4 |
| `highpass` | sharpness, contrast boost | 2 |
| `lowpass` | radius, contrast, brightness, saturation, Gaussian/bilateral algorithm | 5 |
| `soften` | size, saturation, brightness, mix | 4 |
| `monochrome` | virtual filter a/b coordinates, size, highlights | 4 |
| `graduatednd` | density, hardness, rotation, offset, hue, saturation | 6 |
| `vignette` | fall-off start/radius, brightness, saturation, horizontal/vertical center, ratio, shape, automatic ratio, dithering | 10 |
| `splittoning` | shadows/highlights hue and saturation, balance, compress | 6 |
| `bloom` | size, threshold, strength | 3 |
| `grain` | scale, strength, mid-tones bias | 3 |
| `colorcorrection` | saturation | 1 |
| `colorbalance` | input/output saturation, contrast, contrast fulcrum, processing mode | 5 |

Missing introspection bounds are restricted to the reviewed desktop domains:
monochrome a/b `[-128,128]`, filter size `[0.5,3]`, and graduated-filter offset
`[0,100]`. Vignette ratio currently uses `[0.001,1.999]` to exclude the native
manual-ratio division singularities at 0 and 2. Other imported values remain
visible; listing a value is not permission to write an unsafe value. Arbitrary
array members and unreviewed nested fields remain inaccessible. The four exact
nested paths currently reviewed are vignette `center.x`/`center.y` and global
tonemap `drago.bias`/`drago.max_light`.

Some settings depend on another selection: Drago bias/target affect the Drago
operator, and vignette manual ratio affects the manual-ratio path. The engine
keeps these original dependencies; exposing a control does not make it affect
every processing mode. Deprecated desktop modules retain their original status.

## Dedicated editors

| Module or feature | Available interaction | Still pending |
| --- | --- | --- |
| `basecurve`, `rgbcurve` | Drag/add/remove/reset nodes, numeric coordinates, native interpolation choices; RGB linked/independent mode and preserve-color menu | Other advanced settings and other curve modules |
| `crop` | Bounds diagram, four bounded sliders, exact coordinates, reset/apply | Full desktop crop/aspect/rotation interaction |
| `temperature` | Native preset/settings menu, channel-gain sliders and inputs, Kelvin/tint sliders and inputs when native camera metadata permits; atomic Apply | Full preset/tuning and picker interaction |
| Module instances | Create image-specific default instance, independent controls, topology undo/redo | Copy current settings, delete/reorder and full mask handling |
| Export | Native format/options discovery, destination picker and existing advanced export controls | Full optional codec/format parity; see [format coverage](dependencies/FORMAT_PARITY.md) |

## Remaining work

Drawn/parametric/raster masks, blending and mask-manager actions still require
their original desktop state operations and canvas controls. Merely enabling
`mask_manager` does not create an editable mask. Styles and presets, pickers,
linked channel arrays, additional curves, geometry handles and advanced tool
settings also remain pending. They must be implemented with the original core
operations and their invariants, without substitute image-processing formulas.

The `mask_manager` IOP itself is the desktop's hidden mask-history sentinel;
its private `dummy` integer is not a mask editor. The real desktop panel is
`src/libs/masks.c` and edits native forms and groups. Do not expose that dummy
field to turn the current placeholder into a functional-looking control. A
subsequent UI cleanup should hide internal IOPs while preserving their native
history identities. A separate per-instance uniform-blending editor can use
the original `blend_params` and `dt_iop_commit_blend_params` with native history;
drawn/parametric/raster modes need their additional original state operations.

The current configured runtime contains 93 original processing modules, including
internal pipeline and diagnostic modules. Many still have no mobile parameter
editor. Major outstanding tools include `filmicrgb`, `agx`, `toneequal`,
`channelmixerrgb`, `colorequal`, `colorzones`, `denoiseprofile`, `diffuse`,
`highlights`, `demosaic`, `lens`, `ashift`, `retouch`, `liquify`, `negadoctor`,
`levels`, `rgblevels`, `hazeremoval` and their dependent controls. A module with
one reviewed field can still have many other unported widgets.

See [port status](dependencies/PORTING_STATUS.md) and
[Android execution tests](../android/NATIVE_ENGINE_TESTS.md) for the exact tested
fixtures and platforms. Build success, UI interaction, native pixel changes and
physical-device behavior are separate validation scopes.
