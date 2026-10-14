# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Blender integration prototype.
# This add-on is intended to run inside Blender and keeps Blender's native
# transform operators intact. It does not modify Blender's GPL source files.

bl_info = {
    "name": "Hamun Authoring Tools",
    "author": "Hamun Engine contributors",
    "version": (0, 1, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Header > Hamun",
    "description": "Initial Hamun workflow integration for Blender",
    "category": "3D View",
}

import bpy


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


class HAMUN_MT_transform_menu(bpy.types.Menu):
    bl_label = "Hamun"
    bl_idname = "HAMUN_MT_transform_menu"

    def draw(self, context):
        layout = self.layout
        layout.operator("hamun.prepare_scene", icon="OUTLINER_COLLECTION")
        layout.separator()
        layout.label(text="Native Transform Tools")
        layout.operator("transform.translate", text="Move", icon="ORIENTATION_GLOBAL")
        layout.operator("transform.rotate", text="Rotate", icon="DRIVER_ROTATIONAL_DIFFERENCE")
        layout.operator("transform.resize", text="Scale", icon="FULLSCREEN_ENTER")
        layout.separator()
        layout.label(text="Transform gizmos retain Blender behavior")


def draw_hamun_menu(self, context):
    self.layout.menu(HAMUN_MT_transform_menu.bl_idname, text="Hamun")


classes = (
    HAMUN_OT_prepare_scene,
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
