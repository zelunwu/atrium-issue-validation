"""Summarize reproducible GPU timing and RGB quality from the blur experiment."""

from __future__ import annotations

import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


ROOT = Path(__file__).resolve().parent
RUNS = [ROOT / "blur-kawase", ROOT / "blur-kawase-repeat"]


def quality(reference: Path, candidate: Path) -> tuple[float, float]:
    with Image.open(reference) as ref_image, Image.open(candidate) as cand_image:
        ref = ref_image.convert("RGB")
        cand = cand_image.convert("RGB")
        if ref.size != cand.size:
            raise ValueError(f"Image size mismatch: {candidate}")
        diff = ImageChops.difference(ref, cand)
        stat = ImageStat.Stat(diff)
        mae = sum(stat.mean) / 3
        mse = sum(stat.sum2) / (ref.width * ref.height * 3)
        psnr = 10 * math.log10(255 * 255 / mse) if mse else math.inf
        return psnr, mae


def main() -> None:
    timings: dict[tuple[str, int, int, int, int, str], list[float]] = defaultdict(list)
    per_run: dict[tuple[str, int, int, int, int, str], list[float]] = defaultdict(list)
    for run in RUNS:
        with (run / "timings.csv").open(newline="", encoding="utf-8") as source:
            rows = list(csv.DictReader(source))
        grouped: dict[tuple[str, int, int, int, int, str], list[float]] = defaultdict(list)
        for row in rows:
            key = (row["algorithm"], int(row["width"]), int(row["height"]),
                   int(row["downsample"]), int(row["passes"]), row["offset_scale"])
            grouped[key].append(float(row["gpu_ms"]))
        for key, values in grouped.items():
            timings[key].extend(values)
            per_run[key].append(statistics.median(values))

    print("algorithm,resolution,downsample,passes,offset,median_ms,run_medians_ms,psnr_db_640x320,mae_255_640x320")
    for key in sorted(timings, key=lambda k: (k[1], k[2], k[0], k[3], k[4], k[5])):
        algorithm, width, height, downsample, passes, offset = key
        if algorithm == "atrium":
            psnr = "reference"
            mae = "reference"
        else:
            filename = f"kawase_{downsample}x_{passes}pass_{float(offset):.1f}offset.png"
            score, error = quality(RUNS[0] / "atrium_blur.png", RUNS[0] / filename)
            psnr = f"{score:.2f}"
            mae = f"{error:.2f}"
        medians = "/".join(f"{m:.4f}" for m in per_run[key])
        print(f"{algorithm},{width}x{height},{downsample},{passes},{offset},"
              f"{statistics.median(timings[key]):.4f},{medians},{psnr},{mae}")


if __name__ == "__main__":
    main()
