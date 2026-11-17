# Atrium issue #95 visual and performance baseline

This is an initial, deterministic visual regression baseline for the **CPU glass preview** in Atrium v2.7.0 (`63ea4ed`). It directly compiles the current `GlassPreview.cs` and `CornerProfile.cs` as linked source files; the synthetic photo and simple settings are local stand-ins. The test does **not** execute `glass.hlsl`, a D3D11 device, desktop capture, DirectComposition, or the application UI. Therefore its timings are CPU preview timings, not GPU frame times or end-to-end product performance.

The `expected/` directory holds the approved PNGs for three 640×320 scenes: liquid, frosted, and pill. The first run was generated on Windows 10.0.26200 with .NET SDK 10.0.302 and reg-cli 0.19.0-rc3 from source commit `6290eb7`. The test background is synthetic; there is no captured personal desktop content. The backgrounds deliberately include sharp vertical and horizontal edges so refraction seams are visible.

From this directory, compare the current linked Atrium source against the baseline:

```powershell
.\Run-Comparison.ps1
```

Open `report.html` to inspect the original, new, and difference views. `report.json` and `report.xml` provide machine-readable results. `actual/timings.json` contains the median of five CPU preview draws after one warmup; timings are descriptive only and are not a pass/fail performance gate. Run `-Rebaseline` only after reviewing an intended visual change; it overwrites `expected/`.

The comparator source lives in a sibling `reg-cli` checkout beside this repository, outside Atrium. Atrium's working tree and CI configuration are untouched.

## Native graphics baseline

`native/` builds a C++ executable against Atrium's current source. CMake compiles the original `glass.hlsl`, `backdrop_edge.hlsl`, and `compare.hlsl` with the Windows SDK shader compiler. The executable runs them through D3D11, using the hardware adapter when available and WARP otherwise. It also calls the original C++ geometry, Direct2D and DirectWrite renderers, `BarPainter`, all supported widget kind/size/material combinations, the `Canvas` drawing methods, and controlled popup scenes. The Atrium checkout remains unchanged. For windowed UI scenes, CMake generates temporary copies of selected sources with a readback hook, synthetic inputs, and suppressed background readers. These generated files live under `native/build/`.

After Visual Studio C++ and Windows SDK are installed, run:

```powershell
.\Run-GpuComparison.ps1
```

The script builds the executable, generates **147 numbered PNGs** (`000`–`146`), checks that each was freshly generated, and compares them with `gpu-expected/`. Open `gpu-report.html` for a side by side and difference view; `gpu-report.json` and `gpu-report.xml` contain machine-readable results. To run just the image generator after building:

```powershell
.\native\build\Release\atrium_graphics_baseline.exe .\my-output
```

On an intentional graphics change, inspect the generated output first, then run `Run-GpuComparison.ps1 -Rebaseline` to replace the reference images. `native/CMakeLists.txt` accepts `-DATRIUM_ROOT=<path>` when the Atrium checkout is elsewhere. `gpu-expected/manifest.txt` lists the numbered coverage. The initial GPU reference was generated on an NVIDIA GeForce RTX 5070. Hardware, driver, and Windows rendering differences can change pixels, so review differences on other machines before accepting a new reference.

These are controlled component and window-content checks. The executable calls the original `GlassRenderer::UpdateBackdrop` and both `Present` overloads with synthetic input; a generated-source readback hook records its swap-chain buffer before Windows composition. The image for `WindowPreview` includes the card overlay but not the separate DWM thumbnail visual. Live desktop capture, composed screen output, Stage Manager window movement, and system interaction states still need a separate integration environment. No GPU frame-time claim is made from these images.

See `COVERAGE.md` for an interface-by-interface audit of what the numbered images exercise and what remains untested.

The GPU timestamp profiler is documented in `PERFORMANCE.md`. It writes per-frame CSV data for static and changing synthetic backdrops at several resolutions; the profiler is separate from the numbered image comparison. `AB-OPTIMIZATIONS.md` records the A/B results and acceptance decisions for the candidate shader changes.

## Independent checkout and evidence

Clone this repository beside `atrium-desktop`. The native harness defaults to that sibling checkout; use `-DATRIUM_ROOT=<Atrium checkout>` if it is elsewhere. The CPU preview project accepts `-p:AtriumRoot=<Atrium checkout>`. Both comparison scripts expect a built `reg-cli` in a sibling checkout beside this repository, or the `ATRIUM_REG_CLI` environment variable can point to its CLI file. The reference setup uses [`reg-viz/reg-cli`](https://github.com/reg-viz/reg-cli) at commit `6290eb7` (`npm ci`, then `npm run build`). Native builds need Visual Studio C++, CMake, and the Windows SDK shader compiler; the CPU preview needs .NET 10. Python image analysis needs Pillow and NumPy.

The committed `expected/` and `gpu-expected/` contain only synthetic images. `evidence/2026-11-17/` archives the raw GPU A/B timings, the visual comparison JSON, and selected synthetic blur images. [GLASS-SHADER-AB-2026-11-17.md](GLASS-SHADER-AB-2026-11-17.md) explains the two retained shader optimizations. [BLUR-KAWASE.md](BLUR-KAWASE.md) documents the evaluated Kawase alternative, which was not applied to Atrium.

For a repeatable native run from this checkout:

```powershell
.\Run-GpuComparison.ps1
python .\check_pixel_parity.py gpu-expected gpu-actual
.\Run-Comparison.ps1
```

The `native/glass_before_blur_branch.hlsl` and `native/glass_before_shadow.hlsl` snapshots include Atrium code under its MIT license; see `LICENSE-ATRIUM`. This repository stores no personal desktop captures and does not change the Atrium project's CI or test requirements.
