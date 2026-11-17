# Validation for Atrium issue #95

Issue: [Skip unused glass backdrop samples and interior shadow work](https://github.com/wynter-yaxley/atrium-desktop/issues/95). Date: 2026-11-17. Isolated PR head: `4b243dc6d51eed0706497dc700abeebe81143aa4` on `dev/issue-95-glass-blur-shadow`, based on the author's `main` at `8d67636`. The product diff is limited to `native/core/render/shaders/glass.hlsl`. Host: Windows 10.0.26200, NVIDIA GeForce RTX 5070, one connected display.

| Check | Result | Evidence |
| --- | --- | --- |
| Author's `./tools/build.ps1 -Zip` | PASS | Native core, settings app, and portable package built at the isolated PR head |
| Native CTest | PASS | 15 / 15 tests at the isolated PR head |
| Settings checks | PASS | SettingsChecks |
| Localization checks | PASS | LocalizationChecks, 766-key table check, and 14 localization checker regressions |
| External GPU visual baseline | PASS | 147 / 147 numbered images regenerated; exact RGBA audit found 0 changed pixels |
| External CPU preview baseline | PASS | 3 / 3 synthetic images |
| Independent GPU performance A/B | PASS | Two A→B / B→A rounds, 20 measured frames per case per variant per round; see `PR-ISOLATED-AB.md` and raw CSV in `evidence/2026-11-17/isolated-pr/` |
| Interactive image report | PASS | [147-image visual report](https://zelunwu.github.io/atrium-issue-validation/issues/95-2026-11-17/visual-report.html) |
| Live desktop visual inspection | SKIP | Another Atrium installation was running. A development instance could take over the desktop due to the single-instance behavior. |
| Other GPU / multi-monitor validation | SKIP | No second GPU or second connected monitor in this environment. |

The #95 isolated A/B compared the author's original `glass.hlsl` (file hash `a1541967749e2b3077d0c42eba8255fec464b573`) with the PR head, using the same external harness. The original file is archived at `native/glass_issue95_base.hlsl`. The first image run had one missing notification popup because the host was in a Windows notification suppression state; the harness now fixes that test condition, and a full fresh 147-image run passed. This harness change is external to Atrium.

The earlier `GLASS-SHADER-AB-2026-11-17.md` and its raw data tested a combined branch that also contained the #92 simple-shape specialization. Those measurements remain as candidate history; the independent PR's performance claims come from `PR-ISOLATED-AB.md`. The separate Kawase filter was not retained because its output differed visibly; see `BLUR-KAWASE.md`.

The GPU timings cover synthetic glass draw stages, not full application frame rate, live capture, or composition. The exact image result applies to these fixtures on this GPU, not every scene or rendering environment.
