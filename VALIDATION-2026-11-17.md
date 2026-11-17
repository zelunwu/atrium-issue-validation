# Validation record for Atrium shader branch

Date: 2026-11-17. Tested Atrium commit: `0418e18a6e9463e3de8200820f956baaf504d00e` on `dev/graphics-performance-pixel-parity` in `zelunwu/atrium-desktop`. The merge with the author's `main` changed history but not the tested source tree: both `edb2971` and `0418e18` have tree `e30710c760f904033797372c830decb17a6d7dee`. Host: Windows 10.0.26200, NVIDIA GeForce RTX 5070, one connected display.

| Check | Result | Evidence |
| --- | --- | --- |
| Author's `./tools/build.ps1 -Zip` | PASS | Native core, settings app and `Atrium-2.7.0-win-x64.zip` built at the exact head |
| Native CTest | PASS | 15 / 15 tests |
| Settings checks | PASS | Round trips, validation, dirty state and display discovery |
| Localization checks | PASS | Loading, traversal, fallback and formatting |
| Localization table check | PASS | 766 source keys |
| Localization checker regressions | PASS | 14 cases |
| Development package build | PASS | `./tools/build.ps1 -Development -Zip` |
| External GPU visual baseline | PASS | 147 / 147 synthetic images; 0 changed RGBA pixels in independent pixel audit |
| External CPU preview baseline | PASS | 3 / 3 synthetic images |
| Controlled GPU performance A/B | PASS | Two A→B / B→A runs for each retained optimization; raw CSV in `evidence/2026-11-17/` |
| Live desktop visual inspection | SKIP | Another Atrium installation is running. Atrium uses a single instance mutex; launching the development package could prompt to take over that running copy and change the user's desktop. No takeover was performed. |
| Other GPU / multi-monitor validation | SKIP | No second GPU or second connected monitor in this environment. |

The A/B result describes the glass draw stage on this GPU and synthetic content. It does not establish an end-to-end frame-rate gain. The Kawase filter's lower standalone time does not establish a product improvement and the image differs; see `BLUR-KAWASE.md`.

The testing code and synthetic reference PNGs are in this repository. The Atrium product branch contains only the retained shader changes on top of the earlier combined performance work. No pull request was submitted for this branch as part of this validation.
