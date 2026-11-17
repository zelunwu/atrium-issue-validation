# Atrium blur and Kawase comparison (2026-11-17)

## Scope and hardware

This is an external experiment. It changes neither the Atrium product repository nor any pull request. The GPU is an NVIDIA GeForce RTX 5070 on Windows 10.0.26200. Timings are D3D11 GPU timestamps in milliseconds, with 3 warmup frames and 20 measured frames per run. Two independent runs were made; medians below are reported per run when they differ. These are GPU timings, not CPU or end-to-end frame timings.

## Current implementation

Atrium's default `blur=0.5` maps to `BlurMix=1` and `BlurLod=3.25` (`native/core/settings.h`, `native/core/bar.cpp`). The glass pixel shader (`native/core/render/shaders/glass.hlsl`) reads the captured backdrop at that mip level with five taps: center weight 0.4, four axis taps weight 0.15 each. `PrepareBackdrop` generates the captured ROI mip chain. The mip chain also supports the comparison path, so its measured cost cannot automatically be assigned entirely to blur.

The existing shader computes the five blur samples even when the blur setting is zero. In the current renderer profile (changing input, two runs, 40 frames per row):

| Resolution | Setting | Glass draw median | GenerateMips median | Whole GPU frame median |
| --- | ---: | ---: | ---: | ---: |
| 1920×1080 | blur 0 | 0.0865 | 0.0140 | 0.1759 |
| 1920×1080 | blur 0.5 | 0.0873 | 0.0137 | 0.1762 |
| 2560×1440 | blur 0 | 0.1402 | 0.0164 | 0.2749 |
| 2560×1440 | blur 0.5 | 0.1424 | 0.0167 | 0.2798 |

An experimental build that removes only the five-tap blur expression reduced glass draw to about 0.0794 ms at 1080p and 0.1265 ms at 1440p with blur 0.5. This is a lower-bound control, not an equivalent visual result. It leaves the mip generation in place and does not isolate a guaranteed product saving.

## Standalone filter comparison

The comparison uses the same synthetic source image and BGRA8 outputs. Its Atrium reference reproduces the five-tap `blurred` expression at LOD 3.25 after `GenerateMips`. The alternative first downsamples by 2× or 4× using four bilinear diagonal samples, applies 1–4 four-sample diagonal Kawase passes with increasing offsets, then upsamples once. Both filter outputs cover the *entire* image; this differs from Atrium's clipped glass geometry. The Kawase shader is an experiment, not a proposed replacement.

| Filter | 1080p GPU ms, runs 1 / 2 | 1440p GPU ms, runs 1 / 2 | PSNR at 640×320 | Mean RGB error / 255 |
| --- | ---: | ---: | ---: | ---: |
| Atrium reference | 0.0587 / 0.0660 | 0.0950 / 0.1064 | reference | reference |
| Kawase 2× downsample, 3 passes, offset scale 1.0 | 0.0385 / 0.0387 | 0.0612 / 0.0616 | 30.01 dB | 5.71 |
| Kawase 4× downsample, 3 passes, offset scale 0.5 | 0.0289 / 0.0287 | 0.0433 / 0.0431 | 27.52 dB | 7.51 |

The half-resolution setting is about 34–41% faster in this full-image **filter-only** test. The quarter-resolution setting is faster again but changes the texture more. Neither reproduces the reference image: the half-resolution candidate has visible differences in repeated stripes and around the white rectangle. PSNR and mean error are for one synthetic image; they do not establish acceptable quality across real desktop content or motion.

This experiment does **not** demonstrate an Atrium frame-rate improvement. Current blur sampling happens only inside the glass shapes, while the standalone Kawase pipeline adds downsample and multiple intermediate render passes. The current renderer's entire glass draw is 0.0873 ms at 1080p and 0.1424 ms at 1440p, with many operations beyond blur. To select Kawase for the product, integrate an ROI-scoped variant, compare matched visual quality and motion, and measure the complete frame on more than one GPU.

## Reproduce and inspect

- `./Run-BlurProfile.ps1` measures the real Atrium render path; `./Run-BlurProfile.ps1 -NoBlurControl` measures the no-taps control build.
- `./native/build/Release/atrium_graphics_baseline.exe --blur-compare blur-kawase` generates filter images and `timings.csv`. Change the output path to `blur-kawase-repeat` for a separate run.
- `python ./analyze_blur_compare.py` prints combined medians, independent-run medians, PSNR and mean absolute RGB error. `blur-kawase-summary.csv` stores this output.
- `blur-kawase/atrium_blur.png` and `blur-kawase/kawase_2x_3pass_1.0offset.png` show the reference and closest tested Kawase candidate. All candidate images and raw timing data are in `blur-kawase/` and `blur-kawase-repeat/`.
- `./Run-GpuComparison.ps1` checks the original 147 numbered visual baselines after the harness changes.

Verification on 2026-11-17: both filter runs completed; their corresponding output PNG hashes matched. `Run-GpuComparison.ps1` rebuilt the external harness and passed all 147 numbered baseline images. The Atrium repository remained clean at commit `378c8f3` on `dev/graphics-performance-pixel-parity`.

Algorithm background: Intel's [GPU blur investigation](https://www.intel.com/content/www/us/en/developer/articles/technical/an-investigation-of-fast-real-time-gpu-based-image-blur-algorithms.html) discusses multi-pass Kawase quality and speed tradeoffs. Google's [Skia Kawase sample](https://skia.googlesource.com/skia/%2B/0d16a70c7cbb/gm/kawase_blur_rt.cpp) provides a second primary implementation reference. Neither external result was treated as an Atrium measurement.
