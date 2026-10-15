# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Blender integration prototype.
# This add-on is intended to run inside Blender and keeps Blender's native
# transform operators intact. It does not modify Blender's GPL source files.

bl_info = {
    "name": "Hamun Authoring Tools",
    "author": "Hamun Engine contributors",
    "version": (0, 5, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Header > Hamun",
    "description": "Initial Hamun workflow integration for Blender",
    "category": "3D View",
}

import bpy
import json
import re
import os
from pathlib import Path
from datetime import datetime, timezone




HAMUN_SCHEMA = 1
HAMUN_DIRECTORIES = ("Assets", "Scenes", "Materials", "Scripts")


def _read_hamun_manifest(path):
    """Accept native .hamun manifests and legacy hamun.project.json files."""
    path = Path(path).resolve()
    if path.suffix.lower() != ".hamun" and path.name != "hamun.project.json":
        raise ValueError("Choose a .hamun project or legacy hamun.project.json file")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict) or data.get("engine") != "Hamun":
        raise ValueError("This is not a Hamun project")
    if data.get("schema_version") != HAMUN_SCHEMA:
        raise ValueError("Unsupported Hamun project schema version")
    scene_relative = data.get("startup_scene")
    if not isinstance(scene_relative, str) or not scene_relative:
        raise ValueError("Missing startup_scene in project")
    root = path.parent
    scene = (root / scene_relative).resolve()
    if not scene.is_relative_to(root) or scene.suffix.lower() != ".blend":
        raise ValueError("Invalid startup scene path")
    if not scene.is_file():
        raise FileNotFoundError("Project scene not found: " + str(scene))
    return path, data, root, scene


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

        folders = HAMUN_DIRECTORIES
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
            # Source scenes stay in Blender's native format. The .hamun file is
            # a genuine project manifest, not a renamed .blend file.
            scene = root / "Scenes" / "Main.blend"
            context.scene["hamun_project"] = True
            context.scene["hamun_schema_version"] = HAMUN_SCHEMA
            context.scene["hamun_project_root"] = str(root)
            saved = bpy.ops.wm.save_as_mainfile(filepath=str(scene))
            if "FINISHED" not in saved:
                raise RuntimeError("Could not save initial Blender scene")
            (root / (name + ".hamun")).write_text(
                json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
        except Exception as exc:
            self.report({"ERROR"}, f"Could not finish Hamun project: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Hamun project created: {root}")
        return {"FINISHED"}



class HAMUN_OT_open_project(bpy.types.Operator):
    """Load the startup scene from a native or legacy Hamun project file"""
    bl_idname = "hamun.open_project"
    bl_label = "Open Hamun Project"
    bl_description = "Open a .hamun project or an existing hamun.project.json project"

    filepath: bpy.props.StringProperty(name="Project File", subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(
        default="*.hamun;*.json", options={"HIDDEN"}
    )

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        try:
            path, data, root, scene = _read_hamun_manifest(self.filepath)
        except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
            self.report({"ERROR"}, f"Cannot open project: {exc}")
            return {"CANCELLED"}

        # Existing projects are read without rewriting their manifest or scene.
        # Blender will prompt to save unsaved changes when applicable.
        result = bpy.ops.wm.open_mainfile(filepath=str(scene))
        if "FINISHED" not in result:
            self.report({"ERROR"}, "Blender could not open the project scene")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Opened Hamun project: {data.get('name', root.name)}")
        return {"FINISHED"}


class HAMUN_OT_upgrade_project(bpy.types.Operator):
    """Create a .hamun manifest for an existing Hamun project non-destructively"""
    bl_idname = "hamun.upgrade_project"
    bl_label = "Convert Legacy Project to .hamun"

    filepath: bpy.props.StringProperty(name="Legacy Project", subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.json", options={"HIDDEN"})

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        try:
            path, data, root, scene = _read_hamun_manifest(self.filepath)
            if path.name != "hamun.project.json":
                raise ValueError("Select a legacy hamun.project.json file")
            destination = root / (root.name + ".hamun")
            if destination.exists():
                raise FileExistsError("Native .hamun project already exists")
            destination.write_text(
                json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
        except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
            self.report({"ERROR"}, f"Conversion failed: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Created {destination.name}; original project preserved")
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



# HScene v1 stores entity hierarchy and WORLD transforms. Geometry, animation,
# material bindings and physics are intentionally out of scope for this schema.
_HSCENE_TYPES = {"MESH", "EMPTY", "CAMERA", "LIGHT", "CURVE", "ARMATURE", "OTHER"}


def _hscene_default_path():
    if bpy.data.filepath:
        return str(Path(bpy.data.filepath).parent / (Path(bpy.data.filepath).stem + ".hscene"))
    return str(Path.home() / "Documents" / "Main.hscene")


def _hscene_document(scene):
    entities = []
    for obj in sorted(scene.objects, key=lambda item: item.name):
        position, rotation, scale = obj.matrix_world.decompose()
        asset_ref = obj.get("hamun_asset")
        entities.append({
            "asset": asset_ref if isinstance(asset_ref, str) else None,
            "id": obj.name,
            "name": obj.name,
            "type": obj.type if obj.type in _HSCENE_TYPES else "OTHER",
            "parent": obj.parent.name if obj.parent else None,
            "transform_space": "world",
            "position": [float(v) for v in position],
            "rotation_quaternion": [float(v) for v in rotation],
            "scale": [float(v) for v in scale],
        })
    return {
        "format": "HamunScene",
        "schema_version": 1,
        "name": scene.name,
        "coordinate_system": "Blender_Z_UP",
        "transform_space": "world",
        "entities": entities,
    }


def _validate_hscene(data):
    import math
    if not isinstance(data, dict) or data.get("format") != "HamunScene":
        raise ValueError("Not a HamunScene document")
    if data.get("schema_version") != 1:
        raise ValueError("Unsupported HScene schema")
    if data.get("coordinate_system") != "Blender_Z_UP":
        raise ValueError("Unsupported coordinate system")
    entities = data.get("entities")
    if not isinstance(entities, list) or len(entities) > 100000:
        raise ValueError("Invalid entity collection")
    names = set()
    for entity in entities:
        if not isinstance(entity, dict):
            raise ValueError("Invalid entity record")
        name = entity.get("id")
        if not isinstance(name, str) or not name or name in names:
            raise ValueError("Duplicate or invalid entity id")
        names.add(name)
        if entity.get("asset") is not None and not isinstance(entity["asset"], str):
            raise ValueError("Invalid asset reference")
        if entity.get("type") not in _HSCENE_TYPES:
            raise ValueError("Unsupported entity type")
        if entity.get("transform_space") != "world":
            raise ValueError("Unsupported transform space")
        for key, size in (("position", 3), ("rotation_quaternion", 4), ("scale", 3)):
            value = entity.get(key)
            if (not isinstance(value, list) or len(value) != size or
                    any(not isinstance(n, (int, float)) or isinstance(n, bool) or
                        not math.isfinite(n) for n in value)):
                raise ValueError("Invalid " + key + " for " + name)
    for entity in entities:
        parent = entity.get("parent")
        if parent is not None and (parent not in names or parent == entity["id"]):
            raise ValueError("Invalid parent for " + entity["id"])
    parents = {e["id"]: e.get("parent") for e in entities}
    for name in names:
        seen = set()
        current = name
        while current is not None:
            if current in seen:
                raise ValueError("Cyclic entity hierarchy")
            seen.add(current)
            current = parents[current]
    return entities



def _hamun_asset_path(scene, reference, root_override=None):
    root = Path(root_override or scene.get("hamun_project_root", "")).resolve()
    if not root_override and not root.is_dir() and bpy.data.filepath:
        root = Path(bpy.data.filepath).resolve().parent.parent
    if not (root / "Assets").is_dir():
        raise ValueError("Hamun project Assets directory not found")
    path = Path(reference)
    if path.is_absolute() or ".." in path.parts or path.suffix.lower() != ".glb":
        raise ValueError("Only relative .glb paths inside Assets are allowed")
    target = (root / path).resolve()
    if not target.is_relative_to((root / "Assets").resolve()) or not target.is_file():
        raise ValueError("GLB asset not found under project Assets")
    return target


class HAMUN_OT_link_glb(bpy.types.Operator):
    """Associate an existing project GLB with the selected mesh object"""
    bl_idname = "hamun.link_glb"
    bl_label = "Link Project GLB"
    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.glb", options={"HIDDEN"})

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.active_object.type == "MESH"

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        try:
            root = Path(context.scene.get("hamun_project_root", ""))
            if not root.is_dir() and bpy.data.filepath:
                root = Path(bpy.data.filepath).resolve().parent.parent
            target = Path(bpy.path.abspath(self.filepath)).resolve()
            reference = target.relative_to(root.resolve()).as_posix()
            _hamun_asset_path(context.scene, reference, root)
            context.active_object["hamun_asset"] = reference
        except (ValueError, OSError) as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}
        self.report({"INFO"}, "Asset linked: " + reference)
        return {"FINISHED"}


class HAMUN_OT_export_hscene(bpy.types.Operator):
    """Export scene entity hierarchy and transforms as a HamunScene file"""
    bl_idname = "hamun.export_hscene"
    bl_label = "Export Hamun Scene (.hscene)"

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.hscene", options={"HIDDEN"})

    def invoke(self, context, event):
        self.filepath = _hscene_default_path()
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        try:
            path = Path(bpy.path.abspath(self.filepath))
            if path.suffix.lower() != ".hscene":
                path = path.with_suffix(".hscene")
            if not path.parent.is_dir():
                raise ValueError("Destination folder does not exist")
            content = json.dumps(_hscene_document(context.scene), indent=2, ensure_ascii=False) + "\n"
            # Avoid overwriting project source files accidentally.
            path.write_text(content, encoding="utf-8")
        except (OSError, ValueError) as exc:
            self.report({"ERROR"}, f"HScene export failed: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Exported {len(context.scene.objects)} HScene entities")
        return {"FINISHED"}


class HAMUN_OT_import_hscene(bpy.types.Operator):
    """Import HScene as transform placeholders, without mesh geometry"""
    bl_idname = "hamun.import_hscene"
    bl_label = "Import Hamun Scene (.hscene)"

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.hscene", options={"HIDDEN"})

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        from mathutils import Matrix, Quaternion, Vector
        try:
            source = Path(bpy.path.abspath(self.filepath))
            if source.suffix.lower() != ".hscene":
                raise ValueError("Choose a .hscene file")
            if source.stat().st_size > 32 * 1024 * 1024:
                raise ValueError("HScene file exceeds 32 MB limit")
            entities = _validate_hscene(json.loads(source.read_text(encoding="utf-8-sig")))
            root = source.resolve().parent.parent if source.parent.name == "Scenes" else source.parent
            assets = {}
            for entity in entities:
                reference = entity.get("asset")
                if reference:
                    assets[reference] = _hamun_asset_path(context.scene, reference, root)
        except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
            self.report({"ERROR"}, f"HScene import failed: {exc}")
            return {"CANCELLED"}

        collection = bpy.data.collections.new("Hamun Import")
        context.scene.collection.children.link(collection)
        mapping = {}
        matrices = {}
        for entity in entities:
            mesh = None
            reference = entity.get("asset")
            if reference:
                previous = set(bpy.data.objects)
                try:
                    result = bpy.ops.import_scene.gltf(filepath=str(assets[reference]))
                    if "FINISHED" not in result:
                        raise RuntimeError("GLB import cancelled")
                    created = set(bpy.data.objects) - previous
                    meshes = [item for item in created if item.type == "MESH"]
                    if len(meshes) != 1:
                        raise ValueError("Only single-mesh GLB assets are supported")
                    mesh = meshes[0].data.copy()
                    for item in created:
                        bpy.data.objects.remove(item, do_unlink=True)
                except Exception as exc:
                    self.report({"ERROR"}, f"GLB import failed: {exc}")
                    return {"CANCELLED"}
            obj = bpy.data.objects.new(entity["name"], mesh)
            if mesh is None:
                obj.empty_display_type = "PLAIN_AXES"
            if reference:
                obj["hamun_asset"] = reference
            obj["hamun_entity_id"] = entity["id"]
            obj["hamun_original_type"] = entity["type"]
            collection.objects.link(obj)
            mapping[entity["id"]] = obj
            pos = Vector(entity["position"])
            quat = Quaternion(entity["rotation_quaternion"])
            quat.normalize()
            sc = Vector(entity["scale"])
            matrices[entity["id"]] = Matrix.LocRotScale(pos, quat, sc)
        for entity in entities:
            if entity["parent"]:
                mapping[entity["id"]].parent = mapping[entity["parent"]]
        # Parent world transforms must be assigned before their children.
        pending = {entity["id"]: entity.get("parent") for entity in entities}
        while pending:
            ready = [name for name, parent in pending.items() if parent not in pending]
            if not ready:
                raise RuntimeError("Unexpected cyclic hierarchy")
            for name in ready:
                mapping[name].matrix_world = matrices[name]
                del pending[name]
        context.view_layer.update()
        self.report({"INFO"}, f"Imported {len(entities)} HScene placeholders (no mesh data)")
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
        layout.operator("hamun.open_project", icon="FILE_FOLDER")
        layout.operator("hamun.upgrade_project", icon="FILE_REFRESH")
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
        layout.operator("hamun.link_glb", icon="LINKED")
        layout.operator("hamun.export_hscene", icon="EXPORT")
        layout.operator("hamun.import_hscene", icon="IMPORT")


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
    HAMUN_OT_open_project,
    HAMUN_OT_upgrade_project,
    HAMUN_OT_prepare_scene,
    HAMUN_OT_setup_workspace,
    HAMUN_OT_export_scene,
    HAMUN_OT_link_glb,
    HAMUN_OT_export_hscene,
    HAMUN_OT_import_hscene,
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
