# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Blender integration prototype.
# This add-on is intended to run inside Blender and keeps Blender's native
# transform operators intact. It does not modify Blender's GPL source files.

bl_info = {
    "name": "Hamun Authoring Tools",
    "author": "Hamun Engine contributors",
    "version": (0, 2, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Header > Hamun",
    "description": "Initial Hamun workflow integration for Blender",
    "category": "3D View",
}

import bpy
import json
import re
from pathlib import Path
from datetime import datetime, timezone



class HAMUN_OT_create_project(bpy.types.Operator):
    """Create a Hamun project and save the current scene into its Scenes folder"""
    bl_idname = "hamun.create_project"
    bl_label = "Create Hamun Project"
    bl_description = "Create a Hamun project folder, manifest, and initial Blender scene"

    project_name: bpy.props.StringProperty(name="Project Name", default="MyHamunProject")
    directory: bpy.props.StringProperty(
        name="Parent Folder",
        subtype="DIR_PATH",
        default="//",
        description="Folder in which to create the new Hamun project",
    )

    def invoke(self, context, event):
        if self.directory == "//" and not bpy.data.filepath:
            self.directory = str(Path.home() / "Documents")
        return context.window_manager.invoke_props_dialog(self, width=460)

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "project_name")
        layout.prop(self, "directory")

    def execute(self, context):
        name = self.project_name.strip()
        if (not name or len(name) > 80 or name in {".", ".."} or
                re.search(r'[<>:"/\\|?*\x00-\x1f]', name) or
                name[-1] in {" ", "."} or
                name.split(".")[0].upper() in
                {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)),
                 *(f"LPT{i}" for i in range(1, 10))}):
            self.report({"ERROR"}, "Invalid Windows project folder name")
            return {"CANCELLED"}

        parent = Path(bpy.path.abspath(self.directory)).expanduser()
        if not parent.is_dir():
            self.report({"ERROR"}, "Choose an existing parent folder")
            return {"CANCELLED"}
        root = parent / name
        if root.exists():
            self.report({"ERROR"}, "Project folder already exists; choose another name")
            return {"CANCELLED"}

        folders = ("Assets", "Scenes", "Materials", "Scripts")
        try:
            root.mkdir()
            for folder in folders:
                (root / folder).mkdir()
            manifest = {
                "schema_version": 1,
                "engine": "Hamun",
                "name": name,
                "created_utc": datetime.now(timezone.utc).isoformat(),
                "startup_scene": "Scenes/Main.blend",
                "asset_directories": list(folders),
            }
            (root / "hamun.project.json").write_text(
                json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            bpy.ops.wm.save_as_mainfile(filepath=str(root / "Scenes" / "Main.blend"))
            context.scene["hamun_project"] = True
            context.scene["hamun_schema_version"] = 1
            context.scene["hamun_project_root"] = str(root)
            # Save again so project metadata persists inside the scene.
            bpy.ops.wm.save_as_mainfile(filepath=str(root / "Scenes" / "Main.blend"))
        except Exception as exc:
            self.report({"ERROR"}, f"Could not finish Hamun project: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Hamun project created: {root}")
        return {"FINISHED"}


class HAMUN_OT_prepare_scene(bpy.types.Operator):
    """Add Hamun metadata and an organized asset collection to this scene"""
    bl_idname = "hamun.prepare_scene"
    bl_label = "Prepare Hamun Scene"
    bl_options = {"REGISTER", "UNDO"}

    collection_name: bpy.props.StringProperty(
        name="Asset Collection",
        description="Collection used to organize assets authored for Hamun",
        default="Hamun Assets",
    )

    def execute(self, context):
        scene = context.scene
        collection = bpy.data.collections.get(self.collection_name)
        if collection is None:
            collection = bpy.data.collections.new(self.collection_name)
        if collection.name not in {child.name for child in scene.collection.children}:
            scene.collection.children.link(collection)

        scene["hamun_project"] = True
        scene["hamun_schema_version"] = 1
        scene["hamun_authoring_profile"] = "blender"
        self.report({"INFO"}, "Hamun scene metadata and asset collection are ready")
        return {"FINISHED"}



class HAMUN_OT_setup_workspace(bpy.types.Operator):
    """Configure the active Blender workspace for Hamun scene authoring"""
    bl_idname = "hamun.setup_workspace"
    bl_label = "Set Up Hamun Workspace"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        workspace = context.window.workspace
        workspace.name = "Hamun Layout"
        context.scene["hamun_project"] = True
        context.scene["hamun_schema_version"] = 1
        for screen in (workspace.screens if hasattr(workspace, "screens") else [context.screen]):
            for area in screen.areas:
                if area.type == "VIEW_3D":
                    space = area.spaces.active
                    space.show_gizmo = True
                    space.show_gizmo_object_translate = True
                    space.show_gizmo_object_rotate = True
                    space.show_gizmo_object_scale = True
                    space.overlay.show_floor = True
                    space.shading.type = "SOLID"
                elif area.type == "PROPERTIES":
                    area.spaces.active.context = "OBJECT"
        self.report({"INFO"}, "Hamun workspace configured using Blender native editors")
        return {"FINISHED"}


class HAMUN_OT_export_scene(bpy.types.Operator):
    """Export the current Blender scene as glTF for Hamun's existing importer"""
    bl_idname = "hamun.export_scene"
    bl_label = "Export Scene to Hamun (glTF)"
    bl_options = {"REGISTER"}

    filepath: bpy.props.StringProperty(name="Export File", subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.glb", options={"HIDDEN"})

    def invoke(self, context, event):
        if not self.filepath:
            import os
            base = bpy.path.abspath("//") if bpy.data.filepath else os.path.expanduser("~")
            self.filepath = os.path.join(base, "HamunScene.glb")
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        if not self.filepath:
            self.report({"ERROR"}, "Choose an output file")
            return {"CANCELLED"}
        result = bpy.ops.export_scene.gltf(
            filepath=self.filepath,
            export_format="GLB",
            use_selection=False,
        )
        if "FINISHED" not in result:
            self.report({"ERROR"}, "glTF export failed")
            return {"CANCELLED"}
        self.report({"INFO"}, "Scene exported for Hamun: " + self.filepath)
        return {"FINISHED"}


class HAMUN_PT_authoring(bpy.types.Panel):
    bl_label = "Hamun Engine"
    bl_idname = "HAMUN_PT_authoring"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Hamun"

    def draw(self, context):
        layout = self.layout
        layout.operator("hamun.create_project", icon="FILE_NEW")
        layout.operator("hamun.setup_workspace", icon="WORKSPACE")
        layout.operator("hamun.prepare_scene", icon="OUTLINER_COLLECTION")
        layout.separator()
        col = layout.column(align=True)
        col.label(text="Native Blender Transform")
        col.operator("transform.translate", text="Move")
        col.operator("transform.rotate", text="Rotate")
        col.operator("transform.resize", text="Scale")
        layout.separator()
        layout.operator("hamun.export_scene", icon="EXPORT")


class HAMUN_MT_transform_menu(bpy.types.Menu):
    bl_label = "Hamun"
    bl_idname = "HAMUN_MT_transform_menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("hamun.create_project", icon="FILE_NEW")
        layout.operator("hamun.prepare_scene", icon="OUTLINER_COLLECTION")
        layout.separator()
        layout.label(text="Native Transform Tools")
        layout.operator("transform.translate", text="Move", icon="ORIENTATION_GLOBAL")
        layout.operator("transform.rotate", text="Rotate", icon="DRIVER_ROTATIONAL_DIFFERENCE")
        layout.operator("transform.resize", text="Scale", icon="FULLSCREEN_ENTER")
        layout.separator()
        layout.operator("hamun.export_scene", icon="EXPORT")


def draw_hamun_menu(self, context):
    self.layout.menu(HAMUN_MT_transform_menu.bl_idname, text="Hamun")


classes = (
    HAMUN_OT_create_project,
    HAMUN_OT_prepare_scene,
    HAMUN_OT_setup_workspace,
    HAMUN_OT_export_scene,
    HAMUN_PT_authoring,
    HAMUN_MT_transform_menu,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.VIEW3D_MT_editor_menus.append(draw_hamun_menu)


def unregister():
    bpy.types.VIEW3D_MT_editor_menus.remove(draw_hamun_menu)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
