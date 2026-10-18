# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Editor portable startup integration.

import bpy
import sys
import addon_utils
from pathlib import Path


def enable_hamun():
    """Repair existing but not registered add-ons on every fresh editor launch."""
    if bpy.app.background:
        return None

    scripts_dir = Path(bpy.utils.resource_path("LOCAL")) / "scripts"
    addons_dir = scripts_dir / "addons"
    if not addons_dir.is_dir():
        print("HAMUN: Add-ons directory not found:", addons_dir)
        return None
    if str(addons_dir) not in sys.path:
        sys.path.insert(0, str(addons_dir))

    try:
        addon_utils.modules_refresh()
        # Being listed in preferences.addons does NOT guarantee that the
        # classes are registered. This caused HAMUN_VULKAN to be absent.
        _, loaded = addon_utils.check("hamun_authoring_tools")
        if not loaded:
            addon_utils.enable("hamun_authoring_tools", default_set=False, persistent=True)

        import hamun_authoring_tools as module

        # Older settings can preserve a partial registration of the add-on.
        # Heal only the missing RenderEngine and render-properties panel.
        engine = bpy.types.RenderEngine.bl_rna_get_subclass_py("HAMUN_VULKAN")
        if engine is None:
            bpy.utils.register_class(module.HAMUN_RENDER_VULKAN)
            print("HAMUN: Registered Vulkan render engine")

        panel = bpy.types.Panel.bl_rna_get_subclass_py("HAMUN_PT_vulkan_settings")
        if panel is None and hasattr(module, "HAMUN_PT_vulkan_settings"):
            if not hasattr(bpy.types.Scene, "hamun_vulkan_executable"):
                bpy.types.Scene.hamun_vulkan_executable = bpy.props.StringProperty(
                    name="Runtime Executable", subtype="FILE_PATH")
            if not hasattr(bpy.types.Scene, "hamun_vulkan_fps_cap"):
                bpy.types.Scene.hamun_vulkan_fps_cap = bpy.props.EnumProperty(
                    name="Target FPS",
                    items=[('0', 'Unlimited', ''), ('30', '30', ''),
                           ('60', '60', ''), ('120', '120', ''), ('144', '144', '')],
                    default='0')
            bpy.utils.register_class(module.HAMUN_PT_vulkan_settings)
            print("HAMUN: Registered Vulkan settings panel")

        print("HAMUN: Engine status:",
              bpy.types.RenderEngine.bl_rna_get_subclass_py("HAMUN_VULKAN"))
    except Exception:
        import traceback
        print("HAMUN: Startup registration failure:")
        traceback.print_exc()
    return None

def activate_hamun_keymap_first_run():
    """Activate Hamun on an unsaved fresh profile, preserving existing preferences."""
    if bpy.app.background:
        return None
    try:
        prefs_file = Path(bpy.utils.user_resource("CONFIG", path="userpref.blend"))
        if prefs_file.is_file():
            print("HAMUN: Existing user preferences found; keymap unchanged")
            return None
        keyconfigs = bpy.context.window_manager.keyconfigs
        if keyconfigs.active and keyconfigs.active.name == "Hamun":
            return None
        preset = Path(bpy.utils.resource_path("LOCAL")) / "scripts" / "presets" / "keyconfig" / "Hamun.py"
        if not preset.is_file():
            print("HAMUN: Keymap preset unavailable:", preset)
            return None
        outcome = bpy.ops.preferences.keyconfig_activate(filepath=str(preset))
        if 'FINISHED' in outcome:
            print("HAMUN: Activated Hamun keymap for fresh profile")
        else:
            print("HAMUN: Keymap activation cancelled:", outcome)
    except Exception as exc:
        print("HAMUN: Keymap first-run activation failed:", exc)
    return None


def register():
    if not bpy.app.timers.is_registered(enable_hamun):
        bpy.app.timers.register(enable_hamun, first_interval=1.0)
    if not bpy.app.timers.is_registered(activate_hamun_keymap_first_run):
        bpy.app.timers.register(activate_hamun_keymap_first_run, first_interval=1.5)


def unregister():
    if bpy.app.timers.is_registered(enable_hamun):
        bpy.app.timers.unregister(enable_hamun)
    if bpy.app.timers.is_registered(activate_hamun_keymap_first_run):
        bpy.app.timers.unregister(activate_hamun_keymap_first_run)