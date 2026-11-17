# Atrium issue validation archive

This repository archives reproducible, synthetic visual and GPU performance checks for contributions to [Atrium Desktop](https://github.com/wynter-yaxley/atrium-desktop). Each issue has a dated folder named `issues/<issue-number>-<YYYY-MM-DD>/`. The folder contains its own validation record and pixel comparison test, with the input images and raw measurements needed to inspect the result. No captured personal desktop content is stored here.

| Atrium issue | Dated archive | What it checks |
| --- | --- | --- |
| [#92](https://github.com/wynter-yaxley/atrium-desktop/issues/92) | [issues/92-2026-11-17](issues/92-2026-11-17) | Simple-shape glass shader A/B and 147-image exact pixel comparison |
| [#95](https://github.com/wynter-yaxley/atrium-desktop/issues/95) | [issues/95-2026-11-17](issues/95-2026-11-17) | Blur endpoint and shadow optimizations, full validation, 147-image GPU baseline, CPU preview and Kawase comparison |

Run each folder's documented checks from that folder. The #92 pixel audit can run directly on its archived before/after PNGs. The #95 native harness links against an Atrium source checkout and builds the real shader and renderer code; its README lists the Windows tools and external image comparator. Results from one GPU and synthetic scenes are not full application frame-rate or live-desktop acceptance.

The test harness is MIT licensed. Shader snapshots copied from Atrium retain Atrium's [MIT notice](LICENSE-ATRIUM).
