# SPDX-License-Identifier: GPL-2.0-or-later
"""Hide legacy Blender keymap entries in the Preferences menu (STAGING ONLY).

Does not delete/rename built-in Blender keymaps, does not change the active keymap.
Only modifies USERPREF_MT_keyconfigs.draw in a copied portable test package.
Dry-run by default.
"""
import argparse
from pathlib import Path

OLD = b'''class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "preferences.keyconfig_activate"

    def draw(self, context):
        Menu.draw_preset(self, context)
'''

NEW = b'''class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "preferences.keyconfig_activate"

    def draw(self, context):
        # Hamun: preserve original preset files for core compatibility, but
        # hide legacy names in the menu presented to end users.
        import os
        seen = set()
        layout = self.layout
        for folder in bpy.utils.preset_paths("keyconfig"):
            if not os.path.isdir(folder):
                continue
            for filename in sorted(os.listdir(folder)):
                if not filename.endswith(".py"):
                    continue
                name = os.path.splitext(filename)[0]
                if name in {"Blender", "Blender_27x"} or name in seen:
                    continue
                seen.add(name)
                op = layout.operator(
                    "preferences.keyconfig_activate",
                    text=name.replace("_", " "),
                    translate=False,
                )
                op.filepath = os.path.join(folder, filename)
'''

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("package_dir", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    root = args.package_dir.resolve()
    target = root / "5.4" / "scripts" / "startup" / "bl_ui" / "space_userpref.py"
    if not target.is_file():
        raise SystemExit(f"Missing UI source: {target}")
    orig = target.read_bytes()
    newline = b"\r\n" if b"\r\n" in orig else b"\n"
    normalized = orig.replace(b"\r\n", b"\n")
    if normalized.count(OLD) == 1:
        updated_normalized = normalized.replace(OLD, NEW)
    elif normalized.count(NEW) == 1:
        print("Already applied; no changes needed.")
        return
    else:
        raise SystemExit("Unexpected menu source; no files changed. Send the class USERPREF_MT_keyconfigs for inspection.")
    updated = updated_normalized.replace(b"\n", newline)
    if not (root / "5.4" / "scripts" / "presets" / "keyconfig" / "Hamun.py").is_file():
        raise SystemExit("Hamun.py preset missing; create it first. No files changed.")
    if not args.apply:
        print("DRY RUN: would hide Blender and Blender 27x in Preferences menu.")
        return
    backup = target.with_name("space_userpref.py.hamun-original.bak")
    if backup.exists() and backup.read_bytes() != orig:
        raise SystemExit(f"Backup conflict: {backup}")
    if not backup.exists():
        backup.write_bytes(orig)
    target.write_bytes(updated)
    print("APPLIED: Keymap menu now excludes two legacy choices; underlying files preserved.")
    print("Select Hamun in Preferences to make it active, then test LMB/G/R/S/Tab.")

if __name__ == "__main__":
    main()
