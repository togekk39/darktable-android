# Processing module adapters

These include fragments extend the **current upstream translation unit** in the
Android integration's temporary build tree. They are not replacement processing
modules. The processor, private parameter declarations, serialization/version
migration, and original static helper algorithms are compiled from upstream.

The narrowly scoped patches in `mobile/patches/iop/` add those includes and expose
only the internal operations needed for a headless editor. Ordinary desktop
configuration never applies these patches or compiles these fragments.

| Module | Reason for an internal adapter |
| --- | --- |
| AgX | Reuse private pivot, gamma and primaries helpers while separating their GTK updates. |
| Exposure | Expose upstream's effective-exposure proxy for the selected headless instance. |
| Filmic RGB | Apply bounded source-range edits, then reuse upstream's output-power calculation. |
| Lens correction | Resolve private Lensfun defaults, capability flags and catalogs; retain crop fallback for an unknown camera body. |
| Liquify | Validate/edit private warp-node records; retain allocation guards for subpixel previews. |
| Retouch / spots | Reuse private mask/form synchronization with the session's develop context. |
| Temperature | Reuse private preset parameters and upstream's current white-balance mode resolution. |
| Tone equalizer | Reuse private filtered-mask sampling, original interpolation solver and LUT evaluation. |
| CLAHE | Enable upstream introspection metadata for the original two parameters. |
| LUT 3D | Let the original readers open retained absolute document-import paths. |

The former `scalepixels` override was removed: current upstream fixes the geometry
using its pixel-aspect-ratio implementation directly.

`mobile/adapters/common/generate_math.py` generates the white-balance numerical
and zone-map helpers from the same reviewed upstream source. It retains no
spectral integration or zone interpolation implementation in the integration
layer. Extraction boundary and signature checks reject incompatible source.

The integration compatibility manifest and exact patch application must succeed
before configuration. Review source/API changes and rerun both build/test paths
before updating that manifest; accepting a new upstream revision is an explicit
maintenance step.
