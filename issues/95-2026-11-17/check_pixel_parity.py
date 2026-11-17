"""Report byte-level RGBA differences between two generated PNG directories."""
from pathlib import Path
import sys

import numpy as np
from PIL import Image


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: python check_pixel_parity.py REFERENCE_DIR CANDIDATE_DIR")
        return 2
    reference, candidate = map(Path, sys.argv[1:])
    names = sorted(p.name for p in reference.glob("*.png"))
    if names != sorted(p.name for p in candidate.glob("*.png")):
        print("Image inventories differ")
        return 2
    changed = 0
    for name in names:
        a = np.asarray(Image.open(reference / name).convert("RGBA"), dtype=np.int16)
        b = np.asarray(Image.open(candidate / name).convert("RGBA"), dtype=np.int16)
        if a.shape != b.shape:
            print(f"{name}: dimensions differ")
            return 2
        delta = np.abs(a - b)
        mask = np.any(delta != 0, axis=2)
        count = int(np.count_nonzero(mask))
        if count:
            changed += 1
            points = [(int(y), int(x), a[y, x].tolist(), b[y, x].tolist())
                      for y, x in np.argwhere(mask)[:5]]
            print(f"{name}: {count} pixels, max channel delta {int(delta.max())}, first {points}")
    print(f"{len(names)} images, {changed} changed")
    return 1 if changed else 0


if __name__ == "__main__":
    raise SystemExit(main())
