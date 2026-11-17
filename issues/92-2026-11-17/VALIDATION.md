# Validation for Atrium issue #92

Issue: [Specialize simple glass shapes to reduce pixel shader work](https://github.com/wynter-yaxley/atrium-desktop/issues/92). Date: 2026-11-17. The final parity-preserving shader was recorded at Atrium commit `378c8f3`; the archived A/B images came from the original and optimized shader variants in the external harness on Windows 10.0.26200 with an NVIDIA GeForce RTX 5070.

| Check | Result | Evidence |
| --- | --- | --- |
| Synthetic GPU image inventory | PASS | 147 numbered images in both `pixel-compare/before/` and `pixel-compare/after/` |
| Exact RGBA pixel comparison | PASS | `./Run-PixelComparison.ps1` reports `147 images, 0 changed` |
| A/B draw-stage performance | PASS | Two corrected A→B / B→A runs; raw CSV in `performance/` and analysis in `PERFORMANCE-AB.md` |
| Live desktop / other GPUs | SKIP | This archived test contains no live desktop capture or measurements from another GPU |

The parity fix retained the original edge arithmetic at the rim. For the simple-shape draw stage, the broader A/B analysis reports a median reduction from about 0.1296 to 0.0899 ms at 1920×1080 and from 0.2181 to 0.1422 ms at 2560×1440. The complex two-part shape remains on the original shader and showed no meaningful gain. These measurements exclude live capture and final composition; they do not establish application frame-rate improvement.

The #92 issue body describes an earlier faster shader candidate that changed one edge pixel in each of two images. This folder archives the later parity-preserving result. Its before/after PNGs also match the approved 147-image reference used in the #95 archive.
