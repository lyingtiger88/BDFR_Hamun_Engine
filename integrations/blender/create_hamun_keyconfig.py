# SPDX-License-Identifier: GPL-2.0-or-later
"""Create an independent Hamun keyconfig based on Blender's intact preset.

No renaming of Blender.py, Blender_27x.py, or blender_default.py.
Run only against a disposable copy of a portable Hamun package.
"""
import argparse
from pathlib import Path

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("package_dir", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    base = args.package_dir.resolve() / "5.4" / "scripts" / "presets" / "keyconfig"
    original = base / "Blender.py"
    destination = base / "Hamun.py"
    generator = base / "keymap_data" / "blender_default.py"
    for path in (original, generator):
        if not path.is_file():
            raise SystemExit(f"Required original file missing: {path}")
    if destination.exists():
        raise SystemExit(f"Refusing to overwrite existing file: {destination}")
    source = original.read_bytes()
    for marker in (b'IDNAME = os.path.splitext(FILENAME)[0]', b'"blender_default.py"', b'keyconfig_init_from_data(kc, keyconfig_data)'):
        if marker not in source:
            raise SystemExit(f"Unexpected upstream preset contents; marker missing: {marker.decode()}")
    print(f"Source: {original}")
    print(f"New independent preset: {destination}")
    print("Preserves Blender.py and blender_default.py unchanged.")
    if not args.apply:
        print("DRY RUN: no changes written")
        return
    destination.write_bytes(source)
    print("CREATED: Hamun.py (preset ID is derived from its own filename).")
    print("Next: test Hamun keyconfig in Preferences and confirm mouse, G/R/S, Tab.")

if __name__ == "__main__":
    main()
