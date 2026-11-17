# Glass shader A/B results (2026-11-17)

This follow-up experiment uses synthetic backdrops and D3D11 GPU timestamps on Windows 10.0.26200 with an NVIDIA GeForce RTX 5070. Each A/B variant was run in A→B and B→A order, with 3 warmups and 20 measured frames per run. The figures below are medians of the changing-backdrop glass draw stage, in milliseconds. They are not whole-app frame rates.

## Candidate 1: avoid unused backdrop samples

When `BlurMix` is zero, the shader now samples only the sharp background. When it is one, it samples only the blurred background. Intermediate values still calculate both and use the same blend expression. The baseline shader always sampled both.

| Setting and resolution | A run 1 / 2 | B run 1 / 2 | Result |
| --- | ---: | ---: | --- |
| Blur 0, 1080p | 0.0870 / 0.0869 | 0.0810 / 0.0817 | B faster in both runs |
| Blur 0, 1440p | 0.1397 / 0.1403 | 0.1304 / 0.1299 | B faster in both runs |
| Blur 0.5, 1080p | 0.0885 / 0.0876 | 0.0856 / 0.0863 | Small draw-stage gain |
| Blur 0.5, 1440p | 0.1413 / 0.1414 | 0.1386 / 0.1374 | Small draw-stage gain |

The default blur setting maps to `BlurMix=1`. Total GPU frame timings varied enough that the small default draw-stage gain should not be described as a confirmed whole-frame gain.

## Candidate 2: avoid exterior shadow work inside the glass

The original shader evaluates another shape distance and an exponential for every covered pixel, then multiplies the result by zero inside the shape. The new branch performs those operations only for pixels on or outside the boundary. Candidate A is the result after candidate 1; B adds this shadow branch.

| Setting and scene | A run 1 / 2 | B run 1 / 2 | Result |
| --- | ---: | ---: | --- |
| Blur 0.5, 1080p basic shape | 0.0855 / 0.0858 | 0.0816 / 0.0825 | B faster in both runs |
| Blur 0.5, 1440p basic shape | 0.1390 / 0.1397 | 0.1330 / 0.1341 | B faster in both runs |
| Blur 0.5, 1080p complex parts | 0.2732 / 0.2742 | 0.2423 / 0.2416 | B faster in both runs |

These are controlled GPU-stage measurements with the same synthetic test scene. The benefit may vary with shape coverage, desktop content, GPU, power state, and composition. It is not an end-to-end acceptance result.

## Visual and build validation

After each candidate, the external native harness compiled the active Atrium shader and generated all 147 synthetic numbered images. `Run-GpuComparison.ps1` passed all 147 against the original approved images, including blur zero, full blur, intermediate blur, and complex shapes. Raw A/B timing CSVs, the visual comparison JSON, and selected synthetic images are under `evidence/2026-11-17/`. The A/B script is `Run-BlurBranchAB.ps1`; the two shader snapshots under `native/` identify A. The native harness can use `-DATRIUM_GLASS_SHADER_OVERRIDE=<file>` to compile either snapshot.

The Atrium product change is confined to `native/core/render/shaders/glass.hlsl`. The separate Kawase experiment is documented in `BLUR-KAWASE.md`; its image differs from Atrium's baseline, so it is not included in the product patch.
