# SPDX-License-Identifier: GPL-2.0-or-later
"""Install Hamun Authoring Tools into the Blender profile used by HamunEditor.

Run:
  HamunEditor.exe --background --factory-startup --python install_hamun_tools.py
"""
import importlib
import shutil
import sys
from pathlib import Path

import bpy

source = Path(__file__).resolve().parent / "hamun_authoring_tools"
if not (source / "__init__.py").is_file():
    raise RuntimeError("Hamun authoring tools source folder is missing: " + str(source))

addons_dir = Path(bpy.utils.user_resource("SCRIPTS", path="addons", create=True))
target = addons_dir / source.name
if target.exists():
    shutil.rmtree(target)
shutil.copytree(source, target)
if str(addons_dir) not in sys.path:
    sys.path.insert(0, str(addons_dir))
importlib.invalidate_caches()
import addon_utils

addon_utils.modules_refresh()
addon_utils.enable("hamun_authoring_tools", default_set=True, persistent=True)
if "hamun_authoring_tools" not in bpy.context.preferences.addons:
    raise RuntimeError("Hamun add-on was copied but not enabled")
bpy.ops.wm.save_userpref()
print("HAMUN_TOOLS_INSTALLED:", target)
