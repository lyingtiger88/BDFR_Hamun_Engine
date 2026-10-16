# SPDX-License-Identifier: GPL-2.0-or-later
"""Guarded rename of Windows side-by-side assembly folders to Hamun.
Dry-run by default; --apply writes only after validating all three input files.
The executable manifest basename is deliberately unchanged in this stage.
"""
import argparse
from pathlib import Path

FILES = (
    "build_files/cmake/macros.cmake",
    "build_files/cmake/platform/platform_win32.cmake",
    "source/creator/CMakeLists.txt",
)
RULES = {
    "build_files/cmake/macros.cmake": (("blender.shared", "hamun.shared"),),
    "build_files/cmake/platform/platform_win32.cmake": (
        ("blender.crt", "hamun.crt"),
        ("blender.shared", "hamun.shared"),
    ),
    "source/creator/CMakeLists.txt": (("blender.shared", "hamun.shared"),),
}


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("blender_source", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    root = args.blender_source.resolve()
    updates = []
    for rel in FILES:
        path = root / rel
        if not path.is_file():
            raise SystemExit(f"Missing source: {path}")
        raw = path.read_bytes()
        new = raw
        for old, replacement in RULES[rel]:
            before = old.encode("ascii")
            after = replacement.encode("ascii")
            count = new.count(before)
            if count == 0:
                if after in new:
                    print(f"ALREADY UPDATED: {rel}, {replacement}")
                    continue
                raise SystemExit(f"Expected marker not found in {rel}: {old}")
            new = new.replace(before, after)
            print(f"{rel}: {old} -> {replacement} ({count} occurrences)")
        if new != raw:
            updates.append((path, raw, new))
    for path, original, _ in updates:
        backup = path.with_name(path.name + ".hamun-libdirs.bak")
        if backup.exists() and backup.read_bytes() != original:
            raise SystemExit(f"Different backup already exists: {backup}; no files written")
    if not args.apply:
        print(f"DRY RUN: would edit {len(updates)} files. Nothing changed.")
        return
    for path, original, updated in updates:
        backup = path.with_name(path.name + ".hamun-libdirs.bak")
        if not backup.exists():
            backup.write_bytes(original)
        path.write_bytes(updated)
    print(f"APPLIED: {len(updates)} files. Reconfigure/rebuild before testing.")


if __name__ == "__main__":
    run()
