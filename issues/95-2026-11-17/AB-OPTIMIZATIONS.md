# Shader optimization A/B audit (2026-11-17)

The Atrium source was tested on Windows 10.0.26200 with an NVIDIA GeForce RTX 5070. The external harness invokes `GlassRenderer::UpdateBackdrop` and `Present` with synthetic BGRA backdrops and D3D11 GPU timestamps. Each case has 3 warmups and 20 measured frames. The final A/B glass runs used the same executable, in A→B then B→A order; each reported value pools 40 measured frames per variant. The environment switch is printed by the profiler so an accidental empty environment variable cannot masquerade as a shader change.

## Candidate decisions

| Candidate | Controlled result | Decision |
| --- | --- | --- |
| Group-shared reduction in `compare.hlsl` | 1920×1080 changing comparison+mip median 1.076→0.049 ms; 2560×1440 1.821→0.075 ms; static scene essentially unchanged | Keep. The prior optimization is in the combined branch. |
| Compiled basic-shape pixel shader with original edge path | 1920×1080 glass draw median 0.1296→0.0899 ms; 2560×1440 0.2181→0.1422 ms. Complex two-part shape 0.2752→0.2748 ms because it keeps the original pixel shader. | Keep. The basic shader is selected only when no parts, tail or wave are active; pixels within the edge band use the original arithmetic for exact image parity. |
| Zero-dispersion sharp-sample merge, runtime branch | 1920×1080 zero-dispersion scene changed roughly 0.126→0.124 ms; other cases varied in both directions. | Drop: no stable gain. |
| Zero-dispersion compiled shader variants | In a separate four-shader experiment, zero-dispersion medians moved by roughly 1–3 microseconds at 1920×1080 and 2560×1440. Individual 2560×1440 rounds did not improve consistently, while two more embedded shaders and selection paths were required. | Drop: benefit is too small and uncertain for the added code. |

The first glass A/B/C attempt was invalid because a PowerShell environment setting produced an empty variable rather than removing it; all three runs selected A. Those files were overwritten after the switch was fixed. The current `ab-glass-A-round*.csv` and `ab-glass-B-round*.csv` are the final build's corrected A/B runs. The `ab-glass-C-round*.csv` files record a separate experimental four-shader build and are not pooled with the final A/B results. `Run-ShaderAB.ps1` reproduces the retained A/B comparison; C's source was removed after the decision.

## Combined synthetic GPU result

| Case | Original GPU stage total | Retained changes GPU stage total |
| --- | ---: | ---: |
| 1920×1080, changing full backdrop | 1.242 ms | 0.174 ms |
| 2560×1440, changing full backdrop | 2.124 ms | 0.285 ms |
| 1920×1080, static backdrop | 0.213 ms | 0.184 ms |

These totals sum GPU timestamps from ROI copy through `Present` queue submission, excluding live capture and subsequent composition latency. They are not application frame rates or an end-to-end acceptance result. The original comparison timings are in `gpu-profile-raw.csv`; the retained build's detailed frames are in the B-round CSVs.

## Pixel audit

The original shader produced the 147 images in `gpu-a/`; the retained build produced `gpu-b/`. Reg-cli's [A/B report](gpu-ab-report.html) passes all 147 images. `check_pixel_parity.py` independently checks every RGBA pixel and reports zero changed images. The initially specialized shader differed at one edge pixel in each of `144_glass_renderer_present.png` and `145_glass_renderer_multi_shape.png`. A generic edge-band fallback to the original shader arithmetic removed both differences while retaining a measured draw-stage speedup. The final optimized output is the expected visual baseline.

## Limits

The synthetic backdrop alternates between static and fully changed frames. Partial desktop changes, live capture, CPU scheduling, the final DWM frame, other GPUs and power states remain unmeasured. Exact image parity is demonstrated for the 147 synthetic fixtures on this GPU, not for every possible input or rendering environment.
