# SPDX-License-Identifier: GPL-2.0-or-later
"""Rename generated Windows executable manifest to Hamun. Dry run by default."""
import argparse
from pathlib import Path

EDITS = {
    "build_files/cmake/platform/platform_win32.cmake": [
        ("blender.exe.manifest", "HamunEditorCore.exe.manifest", 1),
    ],
    "source/creator/CMakeLists.txt": [
        ("blender.exe.manifest", "HamunEditorCore.exe.manifest", 4),
        ("RENAME blender-launcher.exe.manifest", "RENAME HamunEditor.exe.manifest", 1),
    ],
}

def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=Path)
    p.add_argument("--apply", action="store_true")
    args = p.parse_args()
    pending = []
    for rel, rules in EDITS.items():
        path = args.source / rel
        if not path.is_file():
            raise SystemExit(f"File missing: {path}")
        original = path.read_bytes()
        data = original
        for old_text, new_text, expected in rules:
            old, new = old_text.encode(), new_text.encode()
            count = data.count(old)
            if count == 0 and data.count(new) == expected:
                print("ALREADY UPDATED:", rel, new_text)
                continue
            if count != expected:
                raise SystemExit(f"Mismatch {rel}: {old_text} expected {expected}, found {count}; nothing changed.")
            data = data.replace(old, new)
            print(f"{rel}: {count} replacement(s) of {old_text}")
        if data != original:
            pending.append((path, original, data))
    for path, original, _ in pending:
        backup = path.with_name(path.name + ".hamun-exe-manifest.bak")
        if backup.exists() and backup.read_bytes() != original:
            raise SystemExit(f"Backup conflict {backup}; nothing changed")
    if not args.apply:
        print(f"DRY RUN; {len(pending)} files would change.")
        return
    for path, original, data in pending:
        backup = path.with_name(path.name + ".hamun-exe-manifest.bak")
        if not backup.exists():
            backup.write_bytes(original)
        path.write_bytes(data)
    print(f"APPLIED: {len(pending)} files")

if __name__ == "__main__":
    main()
