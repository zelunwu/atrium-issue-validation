## Observation

The glass pixel shader samples both sharp and blurred backdrop colors even when `BlurMix` is 0 or 1. The default blur setting maps to `BlurMix=1`, so the three sharp samples are unused. The shader also evaluates another shape distance and an exponential for the exterior shadow on pixels inside the glass, where the final shadow contribution is zero.

## Proposed change

- At the two blur-mix endpoints, evaluate only the backdrop color used by the output. Preserve the existing blend expression for intermediate values.
- Evaluate the exterior shadow distance and exponential only on or outside the glass boundary.

This is limited to `native/core/render/shaders/glass.hlsl` and keeps the current blur algorithm and visual output. The separate Kawase experiment was not selected because its output differed visibly.

## Controlled results

On Windows 10.0.26200 with an RTX 5070, two A→B / B→A runs with synthetic changing backdrops measured the glass draw stage:

- Skipping unused blur samples at blur 0: 1080p median 0.0870/0.0869 → 0.0810/0.0817 ms; 1440p 0.1397/0.1403 → 0.1304/0.1299 ms.
- At the default blur 0.5, the shadow branch after the blur change: 1080p basic shape 0.0855/0.0858 → 0.0816/0.0825 ms; 1440p basic shape 0.1390/0.1397 → 0.1330/0.1341 ms; 1080p complex parts 0.2732/0.2742 → 0.2423/0.2416 ms.

All 147 synthetic GPU visual fixtures were byte-identical to the approved baseline. The portable build, 15 native tests, settings checks, and localization checks passed. The development package built, but live desktop inspection was skipped because another Atrium installation was running and the application uses a single-instance mutex.

These are glass draw-stage measurements, not full application frame rates. Live capture/composition, other GPUs, and multi-monitor behavior remain unmeasured. The reproducible harness and raw results are archived separately at https://github.com/zelunwu/atrium-issue-validation/tree/main/issues/95-2026-11-17; they are not proposed as an addition to Atrium's repository.

Related: #92 covers the earlier simple-shape shader specialization. I will wait for feedback before opening a pull request.
