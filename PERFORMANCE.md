# Glass comparison GPU profile (2026-11-17)

This profile uses Atrium `63ea4ed` before and after the `compare.hlsl` group reduction change. The executable compiles Atrium's original shader and renderer source; its generated `glass_renderer_baseline.cpp` adds D3D11 timestamp queries around the stages. The Atrium source contains only the shader change. The synthetic backdrops contain no desktop content.

Run the profile from this directory:

```powershell
cmake --build native/build --config Release --target atrium_graphics_baseline
.\native\build\Release\atrium_graphics_baseline.exe --profile .\gpu-profile.csv
```

The run used Windows 10.0.26200, NVIDIA GeForce RTX 5070, 20 measured frames after 3 warmups per case. `static` reuses a backdrop; `changing` alternates two fully different backdrops. Each row in [gpu-profile-raw.csv](gpu-profile-raw.csv) and [gpu-profile-optimized.csv](gpu-profile-optimized.csv) is one frame. The stage named `compare_mips_ms` includes mip generation, compute comparison, and readback copy submission; it does not wait for CPU readback. Values below are medians in milliseconds.

| Case | Before: compare+mips | After: compare+mips | Before: GPU total | After: GPU total |
| --- | ---: | ---: | ---: | ---: |
| 640×320, changing | 0.161 | 0.019 | 0.196 | 0.054 |
| 1920×1080, static | 0.044 | 0.045 | 0.213 | 0.211 |
| 1920×1080, changing | 1.076 | 0.049 | 1.242 | 0.213 |
| 2560×1440, changing | 1.821 | 0.075 | 2.124 | 0.350 |
| 1920×1080, two glass shapes, changing | 1.076 | 0.050 | 1.254 | 0.225 |

The old compute shader performed three global atomics for each changed pixel. The new shader aggregates changed count and maximum per 16×16 group, then submits up to three global atomics per group. It preserves the per-tile maximum for the production one-tile case, the 16 px visual fixture, and arbitrary tile sizes through a fallback. The fixture checks the exact count (79,900), maximum channel difference (64), and every 16 px tile maximum. The 146 PNG visual comparison passed after the change.

These are GPU stage timings with synthetic full-frame changes, not application frame times. Capture, CPU scheduling, DirectComposition latency, partial desktop changes, other GPUs and display setups are not measured. CPU `UpdateBackdrop`+`Present` submission time was about 0.06 ms in this fixture but does not include later CPU readback or window scheduling. The 2560×1440 before run had high tail outliers (p95 about 6 ms); its median was 1.821 ms.
