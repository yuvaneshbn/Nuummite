from __future__ import annotations

import argparse
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path


def _find_repo_root(start: Path) -> Path:
    cur = start.resolve()
    for parent in [cur, *cur.parents]:
        if (parent / "pyproject.toml").exists() or (parent / "setup.py").exists():
            return parent
    return start.resolve()


@dataclass(frozen=True)
class CopyItem:
    label: str
    sources: list[Path]


def _copy_file(src: Path, dst_dir: Path, *, force: bool, dry_run: bool) -> str:
    dst = dst_dir / src.name
    if dst.exists() and not force:
        return f"SKIP  {dst.name} (already exists)"
    if dry_run:
        return f"DRY   {src} -> {dst}"
    dst_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return f"COPY  {src} -> {dst}"


def _expand_webrtc_bin(root: Path) -> list[Path]:
    bin_dir = root / "third_party" / "webrtc_audio_processing" / "bin"
    if not bin_dir.exists():
        return []
    return sorted([p for p in bin_dir.glob("*.dll") if p.is_file()])


def _default_items(root: Path) -> list[CopyItem]:
    third_party = root / "third_party"

    libsodium_dir = third_party / "libsodium"
    libsodium_bin = libsodium_dir / "bin"

    return [
        CopyItem(
            "opus",
            [third_party / "opus" / "opus.dll"],
        ),
        CopyItem(
            "rnnoise",
            [third_party / "rnnoise" / "rnnoise.dll"],
        ),
        CopyItem(
            "portaudio",
            [third_party / "libportaudio" / "libportaudio.dll"],
        ),
        CopyItem(
            "webrtc_audio_processing",
            _expand_webrtc_bin(root),
        ),
        CopyItem(
            "libsodium",
            [
                libsodium_dir / "libsodium.dll",
                libsodium_bin / "libsodium-26.dll",
                libsodium_bin / "libgcc_s_seh-1.dll",
                libsodium_bin / "libwinpthread-1.dll",
            ],
        ),
    ]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Copy Nuummite runtime DLL dependencies into a PyInstaller onedir folder "
            "(defaults to dist/Nuummite/_internal)."
        )
    )
    parser.add_argument(
        "--target",
        type=Path,
        default=None,
        help="Target directory to copy DLLs into (default: dist/Nuummite/_internal).",
    )
    parser.add_argument("--force", action="store_true", help="Overwrite existing DLLs.")
    parser.add_argument("--dry-run", action="store_true", help="Print actions without copying.")

    args = parser.parse_args(argv)

    root = _find_repo_root(Path(__file__).parent)
    target = args.target or (root / "dist" / "Nuummite" / "_internal")
    target = (root / target) if not target.is_absolute() else target

    items = _default_items(root)

    missing: list[Path] = []
    did_anything = False

    print(f"Repo root : {root}")
    print(f"Target    : {target}")

    for item in items:
        sources = [p for p in item.sources if p is not None]
        if not sources:
            continue
        print(f"\n[{item.label}]")
        for src in sources:
            if not src.exists():
                missing.append(src)
                print(f"MISS  {src}")
                continue
            msg = _copy_file(src, target, force=args.force, dry_run=args.dry_run)
            did_anything = did_anything or msg.startswith(("COPY", "DRY"))
            print(msg)

    if missing:
        print("\nMissing files (not copied):")
        for m in missing:
            print(f"- {m}")

    if not did_anything and not args.dry_run:
        print("\nNothing copied (everything already present). Use --force to overwrite.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

