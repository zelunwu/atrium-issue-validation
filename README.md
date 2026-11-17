# Atrium issue validation archive

This repository archives reproducible, synthetic visual and GPU performance checks for contributions to [Atrium Desktop](https://github.com/wynter-yaxley/atrium-desktop). Each issue has a dated folder named `issues/<issue-number>-<YYYY-MM-DD>/`. The folder contains its own validation record and pixel comparison test, with the input images and raw measurements needed to inspect the result. No captured personal desktop content is stored here.

**[Open the visual report index on GitHub Pages](https://zelunwu.github.io/atrium-issue-validation/).** Each issue page opens the same interactive REG comparison interface used by the local HTML reports. GitHub's source-file preview does not execute these reports; use the Pages links.

Offline copies: [download #92 visual report](https://github.com/zelunwu/atrium-issue-validation/releases/download/reports-2026-11-17/atrium-issue-92-2026-11-17-visual.zip) or [download #95 visual report](https://github.com/zelunwu/atrium-issue-validation/releases/download/reports-2026-11-17/atrium-issue-95-2026-11-17-visual.zip). Extract a ZIP and open its `visual-report.html` in a browser. The #92 report is under `pixel-compare/`; the #95 report is at the ZIP root.

| Atrium issue | Dated archive | What it checks |
| --- | --- | --- |
| [#92](https://github.com/wynter-yaxley/atrium-desktop/issues/92) | [issues/92-2026-11-17](issues/92-2026-11-17) · [visual report](https://zelunwu.github.io/atrium-issue-validation/issues/92-2026-11-17/pixel-compare/visual-report.html) | Simple-shape glass shader A/B and 147-image exact pixel comparison |
| [#95](https://github.com/wynter-yaxley/atrium-desktop/issues/95) | [issues/95-2026-11-17](issues/95-2026-11-17) · [visual report](https://zelunwu.github.io/atrium-issue-validation/issues/95-2026-11-17/visual-report.html) | Blur endpoint and shadow optimizations, full validation, 147-image GPU baseline, CPU preview and Kawase comparison |

Run each folder's documented checks from that folder. The #92 pixel audit can run directly on its archived before/after PNGs. The #95 native harness links against an Atrium source checkout and builds the real shader and renderer code; its README lists the Windows tools and external image comparator. Results from one GPU and synthetic scenes are not full application frame-rate or live-desktop acceptance.

The test harness is MIT licensed. Shader snapshots copied from Atrium retain Atrium's [MIT notice](LICENSE-ATRIUM).
The generated REG report includes bundled code from reg-cli under [its MIT notice](LICENSE-REG-CLI).
