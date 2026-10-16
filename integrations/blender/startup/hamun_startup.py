# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Editor portable startup integration.

import bpy
import sys
import addon_utils
from pathlib import Path


def enable_hamun():
    if bpy.app.background:
        return None

    scripts_dir = Path(bpy.utils.resource_path("LOCAL")) / "scripts"
    addons_dir = scripts_dir / "addons"

    if not addons_dir.is_dir():
        print("HAMUN: Add-ons directory not found:", addons_dir)
        return None

    addons_path = str(addons_dir)

    if addons_path not in sys.path:
        sys.path.insert(0, addons_path)

    addon_utils.modules_refresh()

    if "hamun_authoring_tools" not in bpy.context.preferences.addons:
        try:
            addon_utils.enable(
                "hamun_authoring_tools",
                default_set=False,
                persistent=True,
            )
        except Exception as exc:
            print("HAMUN: Failed to activate:", exc)
            return None

    print("HAMUN: Portable authoring tools initialized")
    return None


def register():
    if not bpy.app.timers.is_registered(enable_hamun):
        bpy.app.timers.register(enable_hamun, first_interval=1.0)


def unregister():
    if bpy.app.timers.is_registered(enable_hamun):
        bpy.app.timers.unregister(enable_hamun)