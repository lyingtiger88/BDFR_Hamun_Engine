# SPDX-License-Identifier: GPL-2.0-or-later
"""Apply the FIRST, narrowly scoped Windows executable-name compatibility patch.

Does not change folders, DLL names, build target IDs, or license attribution.
Dry-run by default. Usage:
  py -3 .../rename_windows_executable_refs.py D:/HamunDev/Blender
  py -3 .../rename_windows_executable_refs.py D:/HamunDev/Blender --apply

Apply only to a dedicated Windows source branch, then compile and test.
"""
from __future__ import annotations
import argparse
from pathlib import Path

CHANGES = {
    "intern/ghost/intern/GHOST_SystemWin32.cc": [
        (b'start_from_launcher = strstr(filename, "blender.exe") != nullptr;',
         b'start_from_launcher = strstr(filename, "HamunEditorCore.exe") != nullptr;'),
    ],
    "intern/ghost/intern/GHOST_WindowWin32.cc": [
        (b'char *blender_app = strstr(blender_path, "blender.exe");',
         b'char *blender_app = strstr(blender_path, "HamunEditorCore.exe");'),
        (b'strcpy(blender_app, "blender-launcher.exe");',
         b'strcpy_s(blender_app, sizeof(blender_path) - (blender_app - blender_path), "HamunEditor.exe");'),
    ],
    "source/blender/blenlib/intern/winstuff.cc": [
        (b'blender_app = strstr(blender_path, "blender.exe");',
         b'blender_app = strstr(blender_path, "HamunEditorCore.exe");'),
        (b'strcpy(blender_app, "blender-launcher.exe");',
         b'strcpy_s(blender_app, sizeof(blender_path) - (blender_app - blender_path), "HamunEditor.exe");'),
    ],
    "source/blender/blenlib/intern/system_win32.cc": [
        (b'PathAppendW(executable_path, L"blender-launcher.exe")',
         b'PathAppendW(executable_path, L"HamunEditor.exe")'),
    ],
    "release/windows/icons/winblender.rc": [
        (b'VALUE "OriginalFilename", "blender.exe"',
         b'VALUE "OriginalFilename", "HamunEditorCore.exe"'),
        (b'VALUE "FileDescription", "Blender"',
         b'VALUE "FileDescription", "Hamun Editor Core"'),
        (b'VALUE "ProductName", "Blender"',
         b'VALUE "ProductName", "Hamun Editor"'),
    ],
}

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("blender_source", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    root = args.blender_source.resolve()
    planned = []
    for name, replacements in CHANGES.items():
        path = root / name
        if not path.is_file():
            raise SystemExit(f"Missing file: {path}")
        original = path.read_bytes()
        updated = original
        for old, new in replacements:
            old_count = updated.count(old)
            if old_count == 1:
                updated = updated.replace(old, new)
            elif old_count == 0 and updated.count(new) == 1:
                print(f"ALREADY APPLIED: {name}")
            else:
                raise SystemExit(f"Unexpected source content in {name}; '{old.decode()}' found {old_count} times. Nothing written.")
        if updated != original:
            planned.append((path, original, updated))
            print(f"CHANGE: {name}")
    if not args.apply:
        print(f"DRY RUN: {len(planned)} files would change; no files written. Add --apply to proceed.")
        return 0
    # All inputs validated before changing any source.
    for path, original, _ in planned:
        backup = path.with_name(path.name + ".hamun-rename.bak")
        if backup.exists() and backup.read_bytes() != original:
            raise SystemExit(f"Existing backup differs: {backup}. Nothing written.")
    for path, original, updated in planned:
        backup = path.with_name(path.name + ".hamun-rename.bak")
        if not backup.exists():
            backup.write_bytes(original)
        path.write_bytes(updated)
    print(f"APPLIED: {len(planned)} files. Rebuild and test; backups have suffix .hamun-rename.bak")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
