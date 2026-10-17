# SPDX-License-Identifier: GPL-2.0-or-later
"""Rebrand keyconfig presets inside a COPIED portable package only.

Renames preset modules and shared generator while updating internal file references.
Do not alter upstream source or license text. Back up package before running.
"""
import argparse
from pathlib import Path

def main():
    p=argparse.ArgumentParser()
    p.add_argument("package",type=Path)
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    root=args.package.resolve()/"5.4"/"scripts"
    key=root/"presets"/"keyconfig"
    replacements={
        key/"Blender.py":key/"Hamun.py",
        key/"Blender_27x.py":key/"Hamun_Legacy.py",
        key/"keymap_data"/"blender_default.py":key/"keymap_data"/"hamun_default.py",
    }
    sources=[
        key/"Blender.py",key/"Blender_27x.py",
        key/"keymap_data"/"industry_compatible_data.py",
    ]
    for x in list(replacements)+sources:
        if not x.is_file(): raise SystemExit("Missing input: "+str(x))
    for a,b in replacements.items():
        if b.exists():raise SystemExit("Target already exists: "+str(b))
    changed={}
    for f in sources:
        src=f.read_bytes()
        new=src.replace(b'"blender_default.py"',b'"hamun_default.py"')
        if f in (key/"Blender.py",key/"Blender_27x.py") and src==new:
            raise SystemExit("Missing expected generator reference in: "+str(f))
        changed[f]=new
    print("Plan: 3 renames, 3 reference updates")
    if not args.apply:
        print("DRY RUN: nothing changed");return
    for f in sources:
        if changed[f]!=f.read_bytes(): f.write_bytes(changed[f])
    for a,b in replacements.items():
        a.rename(b)
        print("Branded:",b.relative_to(root))
    print("APPLIED: test keyboard shortcuts, presets and Industry Compatible keyconfig.")

if __name__=="__main__":main()
