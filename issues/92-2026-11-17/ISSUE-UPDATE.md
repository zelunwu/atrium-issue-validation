Update after the edge-parity fix: the initially measured basic-shape shader changed one 8-bit channel level at one edge pixel in each of two images. The retained shader now uses the original edge arithmetic near the rim. The final 147 synthetic before/after PNG pairs have **zero changed RGBA pixels**.

In the corrected two-round A/B runs, the simple-shape glass draw median was about 0.1296 → 0.0899 ms at 1920×1080 and 0.2181 → 0.1422 ms at 2560×1440. The complex two-part shape stayed on the original shader, with no meaningful gain. These are synthetic GPU-stage timings, not application frame rates.

The dated archive for this issue contains the validation record, pixel comparison script, all 147 before/after synthetic image pairs, and raw timing CSV files: https://github.com/zelunwu/atrium-issue-validation/tree/main/issues/92-2026-11-17

I am keeping this issue open for review and will wait before opening a pull request.
