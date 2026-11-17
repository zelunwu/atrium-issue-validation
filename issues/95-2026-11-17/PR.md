Closes #95.

## Change

- At `BlurMix` endpoints, sample only the sharp or blurred backdrop actually used. Intermediate blur values keep the original blend expression.
- Calculate exterior shadow distance and its exponential only on or outside the glass boundary.
- The production diff is one file, `native/core/render/shaders/glass.hlsl`. The blur algorithm is unchanged. The separate Kawase experiment was excluded because it changed the rendered image.

## Validation

- `./tools/build.ps1 -Zip`: passed at isolated head `4b243dc`.
- Native CTest: 15/15 passed. SettingsChecks, LocalizationChecks, the 766-key localization table check, and 14 localization checker regressions passed.
- External D3D11 baseline: all 147 numbered images freshly regenerated from this head; exact RGBA audit found 0 changed pixels. Three CPU preview images also passed.
- [Interactive 147-image comparison](https://zelunwu.github.io/atrium-issue-validation/issues/95-2026-11-17/visual-report.html) · [validation, independent A/B report, and raw CSVs](https://github.com/zelunwu/atrium-issue-validation/tree/main/issues/95-2026-11-17) · [offline visual report ZIP](https://github.com/zelunwu/atrium-issue-validation/releases/tag/reports-2026-11-17).

## Measured performance

Two controlled A→B / B→A rounds compared the author's `main` shader against this PR **without** the #92 specialization. Each case has 40 measured frames per variant after warmups. Pooled medians of the synthetic **glass draw** stage on Windows 10.0.26200 / RTX 5070:

| Blur setting / scene | Original | This PR | Change |
| --- | ---: | ---: | ---: |
| 0, 1920×1080 | 0.1264 ms | 0.1119 ms | −11.5% |
| 0, 2560×1440 | 0.2149 ms | 0.1888 ms | −12.2% |
| 0.5, 1920×1080 | 0.1258 ms | 0.1160 ms | −7.8% |
| 0.5, 2560×1440 | 0.2151 ms | 0.1969 ms | −8.5% |
| 0.5, 1920×1080 complex parts | 0.2720 ms | 0.2413 ms | −11.3% |

Both individual rounds improved in these cases. This measures the two changes together at the GPU draw stage. It does not establish a full application frame-rate gain. Live capture/composition and other GPUs were not measured. Live desktop inspection was skipped because another Atrium installation was running and a development instance could take over the user's desktop.

## Integration

This PR and #97 each start from the author's `main` so their review diffs stay independent. They edit overlapping shader lines; if #97 merges first, I will rebase this branch and resolve the shader conflict before #98 is merged.
