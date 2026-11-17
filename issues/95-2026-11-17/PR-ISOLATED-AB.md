# Independent #95 shader A/B, 2026-11-17

The A variant is the author's `main` shader at `8d67636` (`native/glass_issue95_base.hlsl`), and B is the single-file shader change at `4b243dc`. Neither build includes #92's specialized `PSBasic`. The profiler's printed `basic=1` is only its environment-toggle default; no `PSBasic` entry or renderer selector exists in either #95 build.

The same external D3D11 harness compiled each shader and measured `GlassRenderer::Present` on Windows 10.0.26200 and an NVIDIA GeForce RTX 5070. It used synthetic changing BGRA backdrops, 3 warmups and 20 measured frames per case and variant in each round, with A→B in round 1 and B→A in round 2. The values below are medians of `glass_draw_ms`, in milliseconds. Raw frame CSVs are in `evidence/2026-11-17/isolated-pr/`.

| Blur setting / scene | A round 1 → B | A round 2 → B | Pooled 40-frame A → B | Change |
| --- | ---: | ---: | ---: | ---: |
| 0.00, 1920×1080 | 0.1264 → 0.1121 | 0.1264 → 0.1113 | 0.1264 → 0.1119 | −11.5% |
| 0.00, 2560×1440 | 0.2146 → 0.1888 | 0.2153 → 0.1891 | 0.2149 → 0.1888 | −12.2% |
| 0.00, 1920×1080 complex parts | 0.2711 → 0.2380 | 0.2727 → 0.2372 | 0.2721 → 0.2375 | −12.7% |
| 0.50, 1920×1080 | 0.1262 → 0.1157 | 0.1254 → 0.1165 | 0.1258 → 0.1160 | −7.8% |
| 0.50, 2560×1440 | 0.2150 → 0.1969 | 0.2151 → 0.1968 | 0.2151 → 0.1969 | −8.5% |
| 0.50, 1920×1080 complex parts | 0.2719 → 0.2409 | 0.2732 → 0.2417 | 0.2720 → 0.2413 | −11.3% |

Both optimizations are present in B, so this experiment measures their combined effect relative to the author's original shader. The separate candidate effects on the previous combined branch are archived in `GLASS-SHADER-AB-2026-11-17.md`; their numbers are not used as independent #95 PR gains.

`Run-GpuComparison.ps1` regenerated all 147 numbered GPU images from the isolated #95 branch; `check_pixel_parity.py` found 0 changed RGBA pixels against the approved synthetic baseline. The 3 CPU preview images also passed. The [interactive report](https://zelunwu.github.io/atrium-issue-validation/issues/95-2026-11-17/visual-report.html) and offline ZIP allow image inspection.

These are GPU glass draw-stage measurements. The test did not measure live desktop capture, DWM composition, CPU scheduling, power usage, full application frame rate, other GPUs, or multiple monitors.
