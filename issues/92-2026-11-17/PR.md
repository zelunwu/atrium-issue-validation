Closes #92.

## Change

- Compile a `PSBasic` variant from the existing glass HLSL and select it for a single rounded rectangle without a tail or wave. Complex shapes keep the existing pixel shader.
- Keep the original edge arithmetic within the rim. This resolves the two one-level edge-pixel differences described in the issue while retaining most of the draw-stage gain.
- This PR contains only the simple-shape glass optimization. The comparison-shader optimization was merged separately in #88; blur/shadow work is tracked in #95.

## Validation

- `./tools/build.ps1 -Zip`: passed at `f9fd5a2`.
- Native CTest: 15/15 passed. SettingsChecks, LocalizationChecks, the 766-key localization table check, and 14 localization checker regressions passed.
- External D3D11 baseline: 147/147 numbered images regenerated and exact RGBA comparison found 0 changed pixels. The tested source tree is exactly the same as this PR head (`6935b3f`).
- [Interactive 147-image comparison](https://zelunwu.github.io/atrium-issue-validation/issues/92-2026-11-17/pixel-compare/visual-report.html) · [validation and raw A/B data](https://github.com/zelunwu/atrium-issue-validation/tree/main/issues/92-2026-11-17).

## Measured performance

On Windows 10.0.26200 and an RTX 5070, the controlled A→B / B→A D3D11 timestamp run measured median **glass draw** time for a simple shape:

| Synthetic case | Original | This PR | Change |
| --- | ---: | ---: | ---: |
| 1920×1080 | 0.1296 ms | 0.0899 ms | −30.6% |
| 2560×1440 | 0.2181 ms | 0.1422 ms | −34.8% |

The complex two-part shape keeps the original shader and measured 0.2752→0.2748 ms, within run variation. These timings cover the glass draw stage with synthetic inputs; live capture, composition, application frame rate, and other GPUs were not measured. Live desktop inspection was skipped because another Atrium installation was running and a new instance could take over the user's desktop.
