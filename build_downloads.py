"""Package the committed visual reports and synthetic images for offline viewing."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "downloads"


def add_file(archive: ZipFile, issue: Path, relative: str) -> None:
    source = issue / relative
    if not source.is_file():
        raise FileNotFoundError(source)
    archive.write(source, relative)


def add_images(archive: ZipFile, issue: Path, directory: str, count: int) -> None:
    files = sorted((issue / directory).glob("*.png"))
    if len(files) != count:
        raise RuntimeError(f"{issue / directory}: expected {count} images, found {len(files)}")
    for source in files:
        archive.write(source, f"{directory}/{source.name}")


def package_92() -> Path:
    issue = ROOT / "issues/92-2026-11-17"
    target = OUT / "atrium-issue-92-2026-11-17-visual.zip"
    with ZipFile(target, "w", ZIP_DEFLATED, compresslevel=6) as archive:
        for name in ("README.md", "VALIDATION.md", "Run-PixelComparison.ps1",
                     "pixel-compare/check_pixel_parity.py",
                     "pixel-compare/visual-report.html", "pixel-compare/visual-report.json"):
            add_file(archive, issue, name)
        add_images(archive, issue, "pixel-compare/before", 147)
        add_images(archive, issue, "pixel-compare/after", 147)
    return target


def package_95() -> Path:
    issue = ROOT / "issues/95-2026-11-17"
    target = OUT / "atrium-issue-95-2026-11-17-visual.zip"
    with ZipFile(target, "w", ZIP_DEFLATED, compresslevel=6) as archive:
        for name in ("README.md", "VALIDATION.md", "check_pixel_parity.py",
                     "visual-report.html", "visual-report.json",
                     "cpu-visual-report.html", "cpu-visual-report.json"):
            add_file(archive, issue, name)
        for directory, count in (("gpu-expected", 147), ("visual-actual", 147),
                                 ("expected", 3), ("visual-cpu-actual", 3)):
            add_images(archive, issue, directory, count)
    return target


def main() -> None:
    OUT.mkdir(exist_ok=True)
    for target in (package_92(), package_95()):
        with ZipFile(target) as archive:
            if bad := archive.testzip():
                raise RuntimeError(f"Corrupt ZIP entry: {bad}")
            print(f"{target.name}: {len(archive.namelist())} files, {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
