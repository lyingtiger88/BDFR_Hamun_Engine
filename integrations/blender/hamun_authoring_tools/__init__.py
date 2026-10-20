# SPDX-License-Identifier: GPL-2.0-or-later
# Hamun Blender integration prototype.
# This add-on is intended to run inside Blender and keeps Blender's native
# transform operators intact. It does not modify Blender's GPL source files.

bl_info = {
    "name": "Hamun Authoring Tools",
    "author": "Hamun Engine contributors",
    "version": (0, 8, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Header > Hamun",
    "description": "Initial Hamun workflow integration for Blender",
    "category": "3D View",
}

import bpy
import json
import re
import os
import shutil
import uuid
from pathlib import Path
from datetime import datetime, timezone
import tempfile




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



def _recent_projects_path():
    base = Path(bpy.utils.user_resource("CONFIG", create=True))
    return base / "hamun_recent_projects.json"


def _recent_projects():
    try:
        data = json.loads(_recent_projects_path().read_text(encoding="utf-8"))
        if isinstance(data, list):
            return [str(item) for item in data if isinstance(item, str) and Path(item).is_file()][:12]
    except (OSError, ValueError):
        pass
    return []


def _remember_project(path):
    path = str(Path(path).resolve())
    recent = [path] + [item for item in _recent_projects() if item != path]
    recent = recent[:12]
    target = _recent_projects_path()
    tmp = target.with_suffix(".tmp")
    tmp.write_text(json.dumps(recent, indent=2), encoding="utf-8")
    os.replace(tmp, target)


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
        _remember_project(root / (name + ".hamun"))
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

        _remember_project(path)
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
    root_value = root_override or scene.get("hamun_project_root")
    root = Path(root_value).resolve() if root_value else None
    if root is None and bpy.data.filepath:
        root = Path(bpy.data.filepath).resolve().parent.parent
    if root is None or not (root / "Assets").is_dir():
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
            raw_root = context.scene.get("hamun_project_root")
            root = Path(raw_root) if raw_root else None
            if root is None and bpy.data.filepath:
                root = Path(bpy.data.filepath).resolve().parent.parent
            if root is None:
                raise ValueError("Open a Hamun project first")
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



def _hamun_project_root(context):
    stored = context.scene.get("hamun_project_root")
    if stored:
        root = Path(stored).resolve()
        if (root / "Assets").is_dir() and (root / "Scenes").is_dir():
            return root
    if bpy.data.filepath:
        scene_file = Path(bpy.data.filepath).resolve()
        root = scene_file.parent.parent
        if (root / "Assets").is_dir() and (root / "Scenes").is_dir():
            return root
    raise ValueError("Open a Hamun project before using this command")


class HAMUN_OT_import_asset(bpy.types.Operator):
    """Copy a single-mesh GLB into Assets and place its mesh in the scene"""
    bl_idname = "hamun.import_asset"
    bl_label = "Import Asset"
    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    filter_glob: bpy.props.StringProperty(default="*.glb", options={"HIDDEN"})

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        try:
            root = _hamun_project_root(context)
            source = Path(bpy.path.abspath(self.filepath)).resolve()
            if source.suffix.lower() != ".glb" or not source.is_file():
                raise ValueError("Select an existing .glb file")
            dest = root / "Assets" / source.name
            if source != dest.resolve():
                if dest.exists():
                    dest = root / "Assets" / (source.stem + "_" + uuid.uuid4().hex[:8] + ".glb")
                shutil.copy2(source, dest)
            before = set(bpy.data.objects)
            status = bpy.ops.import_scene.gltf(filepath=str(dest))
            if "FINISHED" not in status:
                raise RuntimeError("Blender GLB import failed")
            created = set(bpy.data.objects) - before
            mesh_objects = [obj for obj in created if obj.type == "MESH"]
            if len(mesh_objects) != 1 or len(created) != 1:
                # Complex GLBs remain imported and editable, but we do not falsely
                # claim their hierarchy is supported by the HScene GLB reloader.
                self.report({"WARNING"}, "GLB imported; automatic HScene link requires one mesh and no extras")
                return {"FINISHED"}
            mesh_objects[0]["hamun_asset"] = dest.relative_to(root).as_posix()
            context.scene["hamun_project_root"] = str(root)
        except (OSError, ValueError, RuntimeError) as exc:
            self.report({"ERROR"}, f"Import Asset: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, "Asset imported and linked automatically")
        return {"FINISHED"}


class HAMUN_OT_save_project(bpy.types.Operator):
    """Save native Blender authoring source and Hamun entity scene together"""
    bl_idname = "hamun.save_project"
    bl_label = "Save Hamun Project"

    def execute(self, context):
        try:
            root = _hamun_project_root(context)
            if not bpy.data.filepath or not Path(bpy.data.filepath).resolve().is_relative_to(root):
                raise ValueError("Current Blender scene is outside the Hamun project")
            # Persist the asset links before creating the engine-facing scene.
            context.scene["hamun_project_root"] = str(root)
            result = bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
            if "FINISHED" not in result:
                raise RuntimeError("Saving Blender source scene failed")
            target = Path(bpy.data.filepath).with_suffix(".hscene")
            target.write_text(
                json.dumps(_hscene_document(context.scene), indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )
        except (OSError, ValueError, RuntimeError) as exc:
            self.report({"ERROR"}, f"Save Project: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, "Saved Blender scene and Hamun .hscene")
        return {"FINISHED"}




class HAMUN_OT_save_project_as(bpy.types.Operator):
    """Copy project files to a new folder and save the currently edited scene there"""
    bl_idname = "hamun.save_project_as"
    bl_label = "Save Hamun Project As"

    project_name: bpy.props.StringProperty(name="New Project Name", default="MyHamunProjectCopy")
    directory: bpy.props.StringProperty(name="Destination Parent Folder", subtype="DIR_PATH")

    def invoke(self, context, event):
        try:
            root = _hamun_project_root(context)
            index = 1
            while (root.parent / f"{root.name}_{index}").exists():
                index += 1
            self.project_name = f"{root.name}_{index}"
            self.directory = str(root.parent)
        except ValueError:
            self.directory = str(Path.home() / "Documents")
        return context.window_manager.invoke_props_dialog(self, width=480)

    def draw(self, context):
        self.layout.prop(self, "project_name")
        self.layout.prop(self, "directory")

    def execute(self, context):
        name = self.project_name.strip()
        if (not name or len(name) > 80 or name in {".", ".."} or
                re.search(r'[<>:"/\\|?*\x00-\x1f]', name) or
                name[-1] in {" ", "."}):
            self.report({"ERROR"}, "Invalid project name")
            return {"CANCELLED"}
        try:
            root = _hamun_project_root(context)
            parent = Path(bpy.path.abspath(self.directory)).resolve()
            if not parent.is_dir():
                raise ValueError("Destination parent folder does not exist")
            destination = parent / name
            if destination.exists():
                raise FileExistsError("Destination already exists; nothing was overwritten")
            if destination.is_relative_to(root):
                raise ValueError("Destination cannot be inside source project")
            manifest_candidates = sorted(root.glob("*.hamun"))
            if not manifest_candidates:
                manifest_candidates = [root / "hamun.project.json"]
            old_manifest, manifest, _, old_scene = _read_hamun_manifest(manifest_candidates[0])
            scene_relative = old_scene.relative_to(root)
            shutil.copytree(root, destination)
            new_manifest = dict(manifest)
            new_manifest["name"] = name
            new_manifest["created_utc"] = datetime.now(timezone.utc).isoformat()
            new_manifest["startup_scene"] = scene_relative.as_posix()
            manifest_dest = destination / (name + ".hamun")
            manifest_dest.write_text(json.dumps(new_manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            for extra in destination.glob("*.hamun"):
                if extra != manifest_dest:
                    extra.unlink()
            context.scene["hamun_project_root"] = str(destination)
            new_scene = destination / scene_relative
            result = bpy.ops.wm.save_as_mainfile(filepath=str(new_scene))
            if "FINISHED" not in result:
                raise RuntimeError("Could not save copied source scene")
            (new_scene.with_suffix(".hscene")).write_text(
                json.dumps(_hscene_document(context.scene), ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
            _remember_project(manifest_dest)
        except (OSError, ValueError, RuntimeError) as exc:
            self.report({"ERROR"}, f"Save As failed: {exc}")
            return {"CANCELLED"}
        self.report({"INFO"}, f"Project copied to {destination}")
        return {"FINISHED"}


class HAMUN_OT_recent_projects(bpy.types.Operator):
    """Open a small project switcher listing recently used Hamun projects"""
    bl_idname = "hamun.recent_projects"
    bl_label = "Recent Hamun Projects"

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width=520)

    def draw(self, context):
        recent = _recent_projects()
        if not recent:
            self.layout.label(text="No recent Hamun projects")
        for path in recent:
            row = self.layout.row()
            button = row.operator("hamun.open_project", text=Path(path).stem, icon="FILE_FOLDER")
            button.filepath = path

    def execute(self, context):
        return {"FINISHED"}


class HAMUN_OT_quick_save(bpy.types.Operator):
    """Save a Hamun project or fall back to Blender's native save"""
    bl_idname = "hamun.quick_save"
    bl_label = "Save Project"

    def execute(self, context):
        try:
            _hamun_project_root(context)
        except ValueError:
            if bpy.data.filepath:
                return bpy.ops.wm.save_mainfile()
            return bpy.ops.wm.save_as_mainfile("INVOKE_DEFAULT")
        return bpy.ops.hamun.save_project()


def draw_hamun_file_menu(self, context):
    layout = self.layout
    layout.separator()
    layout.label(text="Hamun Project")
    layout.operator("hamun.create_project", text="New Hamun Project", icon="FILE_NEW")
    layout.operator("hamun.open_project", text="Open Hamun Project", icon="FILE_FOLDER")
    layout.operator("hamun.recent_projects", text="Recent Hamun Projects", icon="RECOVER_LAST")
    layout.operator("hamun.save_project_as", text="Save Hamun Project As", icon="DUPLICATE")
    layout.operator("hamun.quick_save", text="Save Hamun Project", icon="FILE_TICK")
    layout.operator("hamun.import_asset", text="Import Asset", icon="IMPORT")


_hamun_keymaps = []


def _register_hamun_shortcuts():
    keyconfig = bpy.context.window_manager.keyconfigs.addon
    if keyconfig is None:
        return
    km = keyconfig.keymaps.new(name="Window", space_type="EMPTY")
    for operator, key in (("hamun.quick_save", "S"), ("hamun.open_project", "O")):
        item = km.keymap_items.new(operator, key, "PRESS", ctrl=True)
        _hamun_keymaps.append((km, item))
    viewmap = keyconfig.keymaps.new(name="3D View", space_type="VIEW_3D")
    toggle = viewmap.keymap_items.new("hamun.toggle_metrics", "F3", "PRESS", ctrl=True, shift=True)
    _hamun_keymaps.append((viewmap, toggle))
    for key, modifier, mode in (("F8", {}, "OFF"), ("F8", {"shift": True}, "BASIC"),
                                ("F8", {"ctrl": True}, "ADVANCED")):
        item = viewmap.keymap_items.new("hamun.set_metrics_mode", key, "PRESS", **modifier)
        item.properties.mode = mode
        _hamun_keymaps.append((viewmap, item))


def _unregister_hamun_shortcuts():
    for km, item in _hamun_keymaps:
        km.keymap_items.remove(item)
    _hamun_keymaps.clear()


class HAMUN_OT_play_runtime(bpy.types.Operator):
    """Export mesh geometry and launch the standalone Vulkan runtime"""
    bl_idname = "hamun.play_runtime"
    bl_label = "Play in Hamun Runtime"
    bl_description = "Export current scene to temporary GLB/HMESH and launch HamunRuntime.exe"

    runtime_exe: bpy.props.StringProperty(name="HamunRuntime.exe", subtype="FILE_PATH")

    def invoke(self, context, event):
        if not self.runtime_exe:
            self.runtime_exe = context.scene.get("hamun_runtime_exe", "")
        return context.window_manager.invoke_props_dialog(self, width=510)

    def draw(self, context):
        self.layout.prop(self, "runtime_exe")

    def execute(self, context):
        import subprocess
        import importlib.util
        exe = Path(bpy.path.abspath(self.runtime_exe)).resolve()
        if not exe.is_file() or exe.name.lower() != "hamunruntime.exe":
            self.report({"ERROR"}, "Select the compiled HamunRuntime.exe")
            return {"CANCELLED"}
        converter = Path(__file__).with_name("glb_to_hmesh.py")
        if not converter.is_file():
            self.report({"ERROR"}, "GLB converter missing from Hamun add-on package")
            return {"CANCELLED"}
        try:
            work = Path(tempfile.mkdtemp(prefix="hamun_play_"))
            glb = work / "scene.glb"
            hmesh = work / "scene.hmesh"
            result = bpy.ops.export_scene.gltf(
                filepath=str(glb), export_format="GLB", use_selection=False)
            if "FINISHED" not in result:
                raise RuntimeError("GLB export failed")
            spec = importlib.util.spec_from_file_location("hamun_glb_converter", converter)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            module.convert(glb, hmesh)
            cap = int(getattr(context.scene, "hamun_vulkan_fps_cap", "0"))
            subprocess.Popen(
                [str(exe), "--scene", str(hmesh), "--fps", str(cap)],
                cwd=str(exe.parent),
            )
            context.scene["hamun_runtime_exe"] = str(exe)
        except Exception as exc:
            self.report({"ERROR"}, "Play failed: " + str(exc))
            return {"CANCELLED"}
        self.report({"INFO"}, "Hamun Vulkan Runtime launched")
        return {"FINISHED"}


def _hamun_renderer_dll(scene=None):
    """Resolve shared Vulkan renderer without an EXE picker."""
    # During development prioritize the freshly built DLL, not a stale
    # portable-editor copy of the same library.
    import sys
    roots = []
    # Select the latest viewport-build DLL before any older runtime package.
    # Both the source checkout and the conventional development directory work.
    for parent in Path(__file__).resolve().parents:
        if (parent / "engine" / "runtime" / "CMakeLists.txt").is_file():
            roots.extend([
                parent / "build" / "viewport" / "Release",
                parent / "build" / "hamun-runtime" / "Release",
            ])
            break
    roots.extend([
        Path("D:/HamunDev/Hamun/build/viewport/Release"),
        Path("D:/HamunDev/Hamun/build/hamun-runtime/Release"),
        Path(sys.executable).resolve().parent,
    ])
    root = Path(__file__).resolve()
    # Development checkout: D:/HamunDev/Hamun/integrations/blender/...

    # Portable editor is a fallback, never override a freshly compiled DLL.
    for parent in root.parents:
        roots.append(parent)
    # Respect prior configured path silently as a migration fallback.
    if scene is not None:
        legacy = getattr(scene, "hamun_vulkan_executable", "")
        if legacy:
            roots.append(Path(bpy.path.abspath(legacy)).resolve().parent)
    # Local development sibling under D:/HamunDev.
    roots.append(Path("D:/HamunDev/Hamun/build/hamun-runtime/Release"))
    for directory in roots:
        dll = directory / "HamunRenderer.dll"
        if dll.is_file():
            return dll
    return None


def _hamun_diagnostic_log(message):
    """Append diagnostics to a stable temp-file path; never hide logger failures."""
    import datetime
    import tempfile
    path = Path(tempfile.gettempdir()) / "hamun_viewport_diagnostics.log"
    line = datetime.datetime.now().astimezone().isoformat(timespec="milliseconds") + " " + str(message)
    with path.open("a", encoding="utf-8", errors="replace") as stream:
        stream.write(line + "\n")
        stream.flush()
    return path


def _hamun_diagnostic_snapshot(context):
    """Collect a point-in-time report from Blender and the loaded add-on."""
    import os
    import platform
    import sys
    import traceback
    from datetime import datetime
    lines = [
        "========== HAMUN DIAGNOSTIC SNAPSHOT BEGIN ==========",
        "timestamp=" + datetime.now().astimezone().isoformat(timespec="seconds"),
        "blender_version=" + str(getattr(bpy.app, "version_string", "unknown")),
        "python_version=" + sys.version.replace("\\n", " "),
        "platform=" + platform.platform(),
        "python_executable=" + str(sys.executable),
        "addon_file=" + str(Path(__file__).resolve()),
        "scene_name=" + str(getattr(context.scene, "name", "<none>")),
        "scene_render_engine=" + str(getattr(context.scene.render, "engine", "<none>")),
    ]
    try:
        engine_cls = bpy.types.RenderEngine.bl_rna_get_subclass_py("HAMUN_VULKAN")
        lines.append("hamun_engine_registered=" + str(engine_cls is not None))
        lines.append("hamun_engine_has_view_draw=" + str(bool(engine_cls and hasattr(engine_cls, "view_draw"))))
    except Exception:
        lines.append("engine_registration_error=" + traceback.format_exc().replace("\\n", " | "))
    try:
        lines.append("renderer_dll_selected=" + str(_hamun_renderer_dll(context.scene)))
        candidates = []
        for parent in Path(__file__).resolve().parents:
            if (parent / "engine" / "runtime" / "CMakeLists.txt").is_file():
                candidates.extend([parent / "build" / "viewport" / "Release" / "HamunRenderer.dll",
                                   parent / "build" / "hamun-runtime" / "Release" / "HamunRenderer.dll"])
                break
        candidates.extend([
            Path("D:/HamunDev/Hamun/build/viewport/Release/HamunRenderer.dll"),
            Path("D:/HamunDev/Hamun/build/hamun-runtime/Release/HamunRenderer.dll"),
            Path(sys.executable).resolve().parent / "HamunRenderer.dll",
        ])
        seen = set()
        for candidate in candidates:
            key = str(candidate)
            if key in seen:
                continue
            seen.add(key)
            if candidate.is_file():
                stat = candidate.stat()
                lines.append("renderer_dll_candidate=%s size=%d modified=%s" % (
                    candidate, stat.st_size, datetime.fromtimestamp(stat.st_mtime).astimezone().isoformat(timespec="seconds")))
            else:
                lines.append("renderer_dll_candidate_missing=" + key)
    except Exception:
        lines.append("dll_resolution_error=" + traceback.format_exc().replace("\\n", " | "))
    try:
        depsgraph = context.evaluated_depsgraph_get()
        mesh_count = 0
        mesh_vertices = 0
        mesh_triangles = 0
        light_count = 0
        for obj in depsgraph.objects:
            if obj.type == "MESH" and not obj.hide_render:
                mesh_count += 1
                evaluated = obj.evaluated_get(depsgraph)
                mesh = evaluated.to_mesh()
                try:
                    if mesh is not None:
                        mesh.calc_loop_triangles()
                        mesh_vertices += len(mesh.vertices)
                        mesh_triangles += len(mesh.loop_triangles)
                finally:
                    evaluated.to_mesh_clear()
            elif obj.type == "LIGHT" and not obj.hide_render:
                light_count += 1
                evaluated = obj.evaluated_get(depsgraph)
                data = evaluated.data
                transform = evaluated.matrix_world
                direction = transform.to_quaternion() @ __import__("mathutils").Vector((0, 0, -1))
                lines.append(
                    "LIGHT name=%r type=%s hide_render=%s energy=%s color=%s angle=%s use_shadow=%s "
                    "direction=(%.6f,%.6f,%.6f) position=(%.6f,%.6f,%.6f)" % (
                        obj.name, getattr(data, "type", "<none>"), obj.hide_render,
                        getattr(data, "energy", "<none>"), tuple(getattr(data, "color", ())),
                        getattr(data, "angle", "<not-applicable>"),
                        getattr(data, "use_shadow", "<property-unavailable>"),
                        direction.x, direction.y, direction.z,
                        transform.translation.x, transform.translation.y, transform.translation.z))
        lines.append("SCENE_GEOMETRY mesh_objects=%d vertices=%d triangles=%d visible_lights=%d" % (
            mesh_count, mesh_vertices, mesh_triangles, light_count))
    except Exception:
        lines.append("depsgraph_snapshot_error=" + traceback.format_exc().replace("\\n", " | "))
    try:
        area_info = []
        screen = getattr(context, "screen", None)
        for area in getattr(screen, "areas", ()) if screen else ():
            if area.type == "VIEW_3D":
                area_info.append("VIEW_3D shading=%s width=%s height=%s" % (
                    area.spaces.active.shading.type, area.width, area.height))
        lines.extend(area_info or ["VIEW_3D_area=not-found"])
    except Exception:
        lines.append("viewport_snapshot_error=" + traceback.format_exc().replace("\\n", " | "))
    lines.append("========== HAMUN DIAGNOSTIC SNAPSHOT END ==========")
    return "\\n".join(lines)


class HAMUN_OT_diagnose_viewport(bpy.types.Operator):
    bl_idname = "hamun.diagnose_viewport"
    bl_label = "Write Vulkan Diagnostic Log"
    bl_description = "Append Blender, scene, Sun, renderer DLL and viewport diagnostics to a text log"

    def execute(self, context):
        import traceback
        try:
            path = _hamun_diagnostic_log(_hamun_diagnostic_snapshot(context))
            self.report({"INFO"}, "Diagnostic snapshot written: " + str(path))
        except Exception:
            try:
                path = _hamun_diagnostic_log("DIAGNOSTIC_SNAPSHOT_FAILED\\n" + traceback.format_exc())
                self.report({"ERROR"}, "Diagnostic failed; details: " + str(path))
            except Exception:
                self.report({"ERROR"}, "Could not write Hamun diagnostic log. Check Windows TEMP folder permissions.")
                return {"CANCELLED"}
        return {"FINISHED"}


# Runtime viewport metrics, separate from the F12 render path.
_hamun_metrics = {"mode": "BASIC", "last": None, "frame_ms": 0.0, "fps": 0.0,
                  "cpu_ms": 0.0, "sample_frames": 0, "sample_start": None}


def _hamun_metrics_refresh_timer():
    """Ask Hamun RenderEngine to redraw even when mouse is motionless."""
    try:
        if _hamun_metrics["mode"] != "OFF":
            for window in bpy.context.window_manager.windows:
                scene = window.scene  # ViewLayer has no scene attribute.
                if scene is None or scene.render.engine != "HAMUN_VULKAN":
                    continue
                for area in window.screen.areas:
                    if area.type == "VIEW_3D" and area.spaces.active.shading.type == "RENDERED":
                        area.tag_redraw()
            # Blender may cache a RenderEngine viewport image after area redraw.
            # Engine.tag_redraw() invalidates that cache explicitly.
            for engine in tuple(_hamun_metrics.get("active_engines", ())):
                try:
                    engine.tag_redraw()
                except ReferenceError:
                    _hamun_metrics["active_engines"].remove(engine)
    except Exception as exc:
        _hamun_metrics["timer_error"] = str(exc)
    return 0.25


class HAMUN_OT_toggle_metrics(bpy.types.Operator):
    bl_idname = "hamun.toggle_metrics"
    bl_label = "Toggle Hamun Performance HUD"
    bl_description = "Show or hide live viewport FPS, frame time and CPU process memory"

    def execute(self, context):
        _hamun_metrics["mode"] = "OFF" if _hamun_metrics["mode"] != "OFF" else "BASIC"
        for window in context.window_manager.windows:
            for area in window.screen.areas:
                if area.type == "VIEW_3D":
                    area.tag_redraw()
        return {"FINISHED"}


class HAMUN_OT_set_metrics_mode(bpy.types.Operator):
    bl_idname = "hamun.set_metrics_mode"
    bl_label = "Hamun HUD Mode"
    mode: bpy.props.EnumProperty(items=[
        ("OFF", "Off", ""), ("BASIC", "Basic", ""), ("ADVANCED", "Advanced", "")])

    def execute(self, context):
        _hamun_metrics["mode"] = self.mode
        _hamun_metrics["last"] = None
        for window in context.window_manager.windows:
            for area in window.screen.areas:
                if area.type == "VIEW_3D":
                    area.tag_redraw()
        return {"FINISHED"}


def _hamun_draw_metrics(width, height, cpu_ms=0.0):
    if _hamun_metrics["mode"] == "OFF":
        return
    import time
    import os
    import ctypes
    import blf
    now = time.perf_counter()
    last = _hamun_metrics["last"]
    if last is not None:
        elapsed = now - last
        if elapsed > 0:
            _hamun_metrics["sample_frames"] += 1
            if _hamun_metrics["sample_start"] is None:
                _hamun_metrics["sample_start"] = last
            span = now - _hamun_metrics["sample_start"]
            if span >= 0.5:
                _hamun_metrics["fps"] = _hamun_metrics["sample_frames"] / span
                _hamun_metrics["frame_ms"] = 1000.0 * span / _hamun_metrics["sample_frames"]
                _hamun_metrics["sample_frames"] = 0
                _hamun_metrics["sample_start"] = now
    _hamun_metrics["last"] = now
    _hamun_metrics["cpu_ms"] = cpu_ms
    working_set = "n/a"
    try:
        class PMC(ctypes.Structure):
            _fields_ = [("cb", ctypes.c_uint32), ("PageFaultCount", ctypes.c_uint32),
                        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
        counters = PMC()
        counters.cb = ctypes.sizeof(counters)
        psapi = ctypes.WinDLL("psapi")
        psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
        psapi.GetProcessMemoryInfo.restype = ctypes.c_int
        kernel = ctypes.WinDLL("kernel32")
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        if psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            working_set = "%.1f MiB" % (counters.WorkingSetSize / 1048576.0)
    except Exception:
        pass
    font = 0
    blf.size(font, 13)
    blf.enable(font, blf.SHADOW)
    blf.shadow(font, 3, 0.0, 0.0, 0.0, 0.9)
    fps = _hamun_metrics["fps"]
    frame_ms = _hamun_metrics["frame_ms"]
    basic = [
        ("%.2f FPS" % fps, (0.57, 1.0, 0.48, 1.0)),
        ("Frame: %.2f ms" % frame_ms, (1.0, 0.72, 0.4, 1.0)),
        ("Draw CPU: %.2f ms" % cpu_ms, (1.0, 1.0, 1.0, 1.0)),
        ("Game: n/a", (0.8, 0.8, 0.8, 1.0)),
        ("RHI Thread: n/a", (0.8, 0.8, 0.8, 1.0)),
        ("GPU Time: n/a", (0.8, 0.8, 0.8, 1.0)),
        ("Refresh: %s" % _hamun_metrics.get("timer_error", "active"), (0.8, 0.8, 0.8, 1.0)),
        ("Draws: %s" % _hamun_metrics.get("draws", "n/a"), (1.0, 1.0, 1.0, 1.0)),
        ("Prims: %s" % _hamun_metrics.get("prims", "n/a"), (1.0, 1.0, 1.0, 1.0)),
        ("Lights: %s | Energy: %s" % (_hamun_metrics.get("lights", "n/a"), _hamun_metrics.get("light_energy", "n/a")), (1.0, 1.0, 1.0, 1.0)),
    ]
    lines = basic
    if _hamun_metrics["mode"] == "ADVANCED":
        lines = [
            ("HAMUN | ADVANCED PROFILER", (0.6, 0.95, 1.0, 1.0)),
            *basic,
            ("Editor RAM: " + working_set, (1.0, 1.0, 1.0, 1.0)),
            ("GRAPHICS QUEUE    Busy / Wait / Idle", (1.0, 0.7, 0.35, 1.0)),
            ("Queue Total       n/a / n/a / n/a", (0.6, 1.0, 0.5, 1.0)),
            ("Mesh Pass         n/a / n/a / n/a", (0.6, 1.0, 0.5, 1.0)),
            ("Offscreen Clear   n/a / n/a / n/a", (0.6, 1.0, 0.5, 1.0)),
            ("COMPUTE QUEUE     Busy / Wait / Idle", (1.0, 0.7, 0.35, 1.0)),
            ("Queue Total       n/a / n/a / n/a", (0.6, 1.0, 0.5, 1.0)),
            ("Dispatches: 0 (no compute submitted)", (0.8, 0.8, 0.8, 1.0)),
            ("GPU timestamps: not instrumented", (0.8, 0.8, 0.8, 1.0)),
            ("Pipeline: Vulkan clear + CPU mesh", (0.8, 0.8, 0.8, 1.0)),
        ]
    # Right-align the HUD inside the 3D region, away from the left toolbar.
    # Width is measured per line so Basic and Advanced layouts both fit.
    right_margin = 68  # leave room for right-side viewport navigation gizmos
    for i, (line, color) in enumerate(lines):
        text_width, _ = blf.dimensions(font, line)
        x = max(76, int(width - right_margin - text_width))
        blf.position(font, x, height - 45 - 17 * i, 0)
        blf.color(font, *color)
        blf.draw(font, line)
    blf.disable(font, blf.SHADOW)


class HAMUN_RENDER_VULKAN(bpy.types.RenderEngine):
    """Hamun Vulkan runtime integration placeholder (not an in-editor renderer yet)."""
    bl_idname = "HAMUN_VULKAN"
    bl_label = "Hamun Vulkan (Preview)"
    bl_use_preview = False
    # Never substitute Eevee's viewport: it masks the result of our custom
    # RenderEngine.view_draw, so scene lights appear to have no effect.
    bl_use_eevee_viewport = False

    def view_update(self, context, depsgraph):
        engines = _hamun_metrics.setdefault("active_engines", [])
        if not any(engine is self for engine in engines):
            engines.append(self)
        if not getattr(self, "_hamun_update_logged", False):
            _hamun_diagnostic_log("RenderEngine.view_update CALLED")
            self._hamun_update_logged = True

    def view_draw(self, context, depsgraph):
        """Experimental live image bridge; scene drawing still CPU rasterized."""
        import ctypes
        import traceback
        import time
        import gpu
        from gpu_extras.batch import batch_for_shader
        try:
            frame_started = time.perf_counter()
            if not getattr(self, "_hamun_draw_logged", False):
                _hamun_diagnostic_log("RenderEngine.view_draw CALLED")
                self._hamun_draw_logged = True
            region = context.region
            if region is None:
                return
            width = max(1, min(int(region.width), 1600))
            height = max(1, min(int(region.height), 1600))
            dll = _hamun_renderer_dll(depsgraph.scene)
            if dll is None:
                raise RuntimeError("HamunRenderer.dll missing")
            native = ctypes.WinDLL(str(dll))
            if getattr(self, "_hamun_loaded_dll", None) != str(dll):
                _hamun_diagnostic_log("ACTIVE_RENDERER_DLL=" + str(dll))
                self._hamun_loaded_dll = str(dll)
            native.hamun_renderer_mesh_camera.argtypes = [
                ctypes.c_uint32, ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_float),
            ]
            native.hamun_renderer_mesh_camera.restype = ctypes.c_int
            native.hamun_renderer_last_error.restype = ctypes.c_char_p
            verts, indices, face_normals = [], [], []
            for obj in depsgraph.objects:
                if obj.type != "MESH" or obj.hide_render:
                    continue
                evaluated = obj.evaluated_get(depsgraph)
                mesh = evaluated.to_mesh()
                try:
                    if mesh is None:
                        continue
                    mesh.calc_loop_triangles()
                    base = len(verts) // 3
                    world = evaluated.matrix_world
                    # Inverse transpose is essential for nonuniform or mirrored scales.
                    normal_matrix = world.to_3x3().inverted_safe().transposed()
                    for vertex in mesh.vertices:
                        pt = world @ vertex.co
                        verts.extend((pt.x, pt.y, pt.z))
                    for tri in mesh.loop_triangles:
                        indices.extend(base + int(j) for j in tri.vertices)
                        normal = (normal_matrix @ tri.normal).normalized()
                        face_normals.extend((normal.x, normal.y, normal.z))
                finally:
                    evaluated.to_mesh_clear()
            if not verts or not indices:
                return
            rv3d = context.space_data.region_3d
            vp = rv3d.perspective_matrix
            matrix = (ctypes.c_float * 16)(
                *(vp[row][column] for column in range(4) for row in range(4)))
            rgba = (ctypes.c_uint8 * (width * height * 4))()
            positions = (ctypes.c_float * len(verts))(*verts)
            faces = (ctypes.c_uint32 * len(indices))(*indices)
            _hamun_metrics["draws"] = 1
            _hamun_metrics["prims"] = len(indices) // 3
            # Collect evaluated Blender lights every frame so transforms and
            # color/energy changes are reflected without restarting the editor.
            lights = []
            for obj in depsgraph.objects:
                if obj.type != "LIGHT" or obj.hide_render:
                    continue
                evaluated = obj.evaluated_get(depsgraph)
                data = evaluated.data
                light_type = {"SUN": 0, "POINT": 1, "SPOT": 2, "AREA": 3}.get(data.type)
                if light_type is None or len(lights) >= 32:
                    continue
                transform = evaluated.matrix_world
                pos = transform.translation
                direction = transform.to_quaternion() @ __import__("mathutils").Vector((0, 0, -1))
                color = data.color
                # ABI slot 11: 1 = cast shadows, -1 = disabled.
                # Zero remains legacy-enabled for older test clients.
                shadow_enabled = bool(getattr(data, "use_shadow", True))
                shadow_flag = 1.0 if shadow_enabled else -1.0
                # Sun has no meaningful position: ABI slot 1 carries its
                # angular diameter (radians) without changing the 12-float ABI.
                sun_angle = float(getattr(data, "angle", 0.0)) if data.type == "SUN" else 0.0
                lights.append((float(light_type), sun_angle if data.type == "SUN" else pos.x, pos.y, pos.z,
                               direction.x, direction.y, direction.z,
                               color.r, color.g, color.b, data.energy, shadow_flag))
            # Record only meaningful light-state changes: this exposes whether
            # the Blender UI actually changes the evaluated Sun parameters.
            sun_state = tuple(
                (tuple(round(v, 4) for v in light[4:7]), round(light[1], 5), round(light[10], 3), light[11])
                for light in lights if light[0] < 0.5
            )
            if getattr(self, "_hamun_last_sun_state", None) != sun_state:
                _hamun_diagnostic_log("SUN_INPUT direction_angle_energy_shadow=" + repr(sun_state))
                self._hamun_last_sun_state = sun_state
            light_data = [float(value) for light in lights for value in light]
            light_array = (ctypes.c_float * len(light_data))(*light_data) if light_data else None
            native.hamun_renderer_mesh_lit.argtypes = [
                ctypes.c_uint32, ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                ctypes.c_uint32,
            ]
            native.hamun_renderer_mesh_lit.restype = ctypes.c_int
            # Prefer Vulkan offscreen rasterization when the updated DLL is installed.
            # Preserve a fallback for systems without a Vulkan-capable GPU.
            gpu_viewport = getattr(native, "hamun_gpu_viewport_render", None)
            # GPU direct-light pipeline currently has no shadow attachment.
            # Prefer the implemented CPU Sun shadow map over silently showing
            # a shadowless scene. Keep GPU active for unshadowed previews.
            has_sun = any(item[0] < 0.5 and item[10] > 0.0 for item in lights)
            if has_sun:
                gpu_viewport = None  # CPU shadow fallback; Vulkan has no shadow sampler.
            route = "CPU_SUN_SHADOW_FALLBACK" if has_sun else ("VULKAN_DIRECT_NO_SHADOW" if gpu_viewport is not None else "CPU_FALLBACK")
            if getattr(self, "_hamun_last_render_route", None) != route:
                _hamun_diagnostic_log("RENDER_ROUTE=" + route)
                self._hamun_last_render_route = route
            if gpu_viewport is not None:
                gpu_viewport.argtypes = [
                    ctypes.c_uint32, ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                    ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                    ctypes.c_uint32, ctypes.POINTER(ctypes.c_float),
                    ctypes.POINTER(ctypes.c_char), ctypes.c_uint32,
                ]
                gpu_viewport.restype = ctypes.c_int
                normal_array = (ctypes.c_float * len(face_normals))(*face_normals)
                gpu_error = ctypes.create_string_buffer(512)
                ok = gpu_viewport(
                    width, height, rgba, ctypes.sizeof(rgba),
                    positions, len(verts) // 3, faces, len(indices),
                    matrix, light_array, len(lights), normal_array,
                    gpu_error, len(gpu_error))
                if not ok:
                    message = gpu_error.value.decode("utf-8", errors="replace")
                    if not getattr(self, "_hamun_gpu_error_logged", False):
                        _hamun_diagnostic_log("Vulkan viewport failed, falling back: " + message)
                        self._hamun_gpu_error_logged = True
                else:
                    self._hamun_gpu_error_logged = False
                    if not getattr(self, "_hamun_gpu_success_logged", False):
                        _hamun_diagnostic_log("VULKAN_GPU_VIEWPORT ACTIVE: hardware rasterization + depth test")
                        self._hamun_gpu_success_logged = True
            else:
                ok = 0
            shadow_quality_fn = getattr(native, "hamun_renderer_mesh_lit_normals_quality", None)
            lit_with_normals = getattr(native, "hamun_renderer_mesh_lit_normals", None)
            if not ok and shadow_quality_fn is not None:
                shadow_quality_fn.argtypes = [
                    ctypes.c_uint32, ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                    ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                    ctypes.c_uint32, ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                ]
                shadow_quality_fn.restype = ctypes.c_int
                normal_array = (ctypes.c_float * len(face_normals))(*face_normals)
                ok = shadow_quality_fn(
                    width, height, rgba, ctypes.sizeof(rgba),
                    positions, len(verts) // 3, faces, len(indices),
                    matrix, light_array, len(lights), normal_array,
                    int(depsgraph.scene.hamun_shadow_map_resolution))
            if not ok and shadow_quality_fn is None and lit_with_normals is not None:
                lit_with_normals.argtypes = [
                    ctypes.c_uint32, ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                    ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                    ctypes.c_uint32, ctypes.POINTER(ctypes.c_float),
                ]
                lit_with_normals.restype = ctypes.c_int
                normal_array = (ctypes.c_float * len(face_normals))(*face_normals)
                ok = lit_with_normals(
                    width, height, rgba, ctypes.sizeof(rgba),
                    positions, len(verts) // 3, faces, len(indices),
                    matrix, light_array, len(lights), normal_array)
            elif not ok and shadow_quality_fn is None:
                ok = native.hamun_renderer_mesh_lit(
                    width, height, rgba, ctypes.sizeof(rgba),
                    positions, len(verts) // 3, faces, len(indices),
                    matrix, light_array, len(lights))
            _hamun_metrics["lights"] = len(lights)
            _hamun_metrics["light_energy"] = round(sum(item[10] for item in lights), 2)
            _hamun_diagnostic_log(
                "RENDER_CALL_RESULT ok=%s route=%s vertices=%d triangles=%d lights=%d "
                "dll=%s" % (bool(ok), route, len(verts) // 3, len(indices) // 3,
                            len(lights), str(dll)))
            if not ok:
                error = native.hamun_renderer_last_error()
                message = error.decode(errors="replace") if error else "DLL draw failed"
                _hamun_diagnostic_log("RENDER_CALL_ERROR " + str(message))
                raise RuntimeError(message)
            # Log a compact, reproducible lighting trace only when light state
            # changes; this distinguishes DLL shading from viewport display.
            light_signature = tuple(round(value, 3) for value in light_data)
            light_changed = light_signature != getattr(self, "_hamun_logged_lights", None)
            if light_changed:
                import hashlib
                import os
                sample_step = max(4, (len(rgba) // 8192 // 4) * 4)
                pixel_sample = bytes(rgba[::sample_step])
                digest = hashlib.sha256(pixel_sample).hexdigest()[:16]
                first_lights = [
                    {"type": int(v[0]), "position": tuple(round(x, 2) for x in v[1:4]),
                     "direction": tuple(round(x, 2) for x in v[4:7]),
                     "color": tuple(round(x, 3) for x in v[7:10]),
                     "energy": round(v[10], 3)} for v in lights[:4]
                ]
                _hamun_diagnostic_log(
                    "LIGHT_TRACE dll=%s bytes=%d lights=%s output_sha=%s "
                    "center_rgba=%s" % (
                        str(dll), os.path.getsize(dll), first_lights, digest,
                        tuple(rgba[(height // 2 * width + width // 2) * 4:
                                   (height // 2 * width + width // 2) * 4 + 4])))
                self._hamun_logged_lights = light_signature
            # Blender GPU expects a shaped buffer for texture upload.
            import numpy as np
            pixels_array = np.ctypeslib.as_array(rgba)
            floats = np.ascontiguousarray(
                pixels_array.reshape((height, width, 4)), dtype=np.float32)
            floats *= (1.0 / 255.0)
            buffer = gpu.types.Buffer("FLOAT", (height, width, 4), floats)
            texture = gpu.types.GPUTexture((width, height), format="RGBA32F", data=buffer)
            shader = gpu.shader.from_builtin("IMAGE")
            batch = batch_for_shader(shader, "TRI_FAN", {
                "pos": ((0, 0), (width, 0), (width, height), (0, height)),
                "texCoord": ((0, 1), (1, 1), (1, 0), (0, 0)),
            })
            gpu.state.blend_set("NONE")
            shader.bind()
            shader.uniform_sampler("image", texture)
            batch.draw(shader)
            _hamun_draw_metrics(width, height, (time.perf_counter() - frame_started) * 1000.0)
            if not getattr(self, "_hamun_success_logged", False):
                _hamun_diagnostic_log("Viewport image drawn " + str((width, height)))
                self._hamun_success_logged = True
        except Exception:
            message = traceback.format_exc()
            if getattr(self, "_hamun_last_error", None) != message:
                _hamun_diagnostic_log(message)
                self._hamun_last_error = message

    def render(self, depsgraph):
        # First genuine in-process Vulkan -> RGBA -> Blender Render Result path.
        # This stage renders a diagnostic GPU clear, NOT scene geometry.
        import ctypes
        scene = depsgraph.scene
        dll = _hamun_renderer_dll(scene)
        if not dll or not dll.is_file():
            self.report({"ERROR"}, "HamunRenderer.dll not found beside Editor or in development build")
            return
        scale = scene.render.resolution_percentage / 100.0
        width = max(1, int(scene.render.resolution_x * scale))
        height = max(1, int(scene.render.resolution_y * scale))
        if width > 4096 or height > 4096:
            self.report({"ERROR"}, "Hamun Vulkan preview supports up to 4096 x 4096")
            return
        try:
            native = ctypes.WinDLL(str(dll))
            native.hamun_renderer_offscreen_clear.argtypes = [
                ctypes.c_uint32, ctypes.c_uint32,
                ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
            ]
            native.hamun_renderer_offscreen_clear.restype = ctypes.c_int
            native.hamun_renderer_last_error.restype = ctypes.c_char_p
            pixels = (ctypes.c_uint8 * (width * height * 4))()
            # Extract evaluated mesh triangles in world coordinates.
            positions = []
            faces = []
            for obj in depsgraph.objects:
                if obj.type != "MESH":
                    continue
                evaluated = obj.evaluated_get(depsgraph)
                mesh = evaluated.to_mesh()
                try:
                    if mesh is None:
                        continue
                    mesh.calc_loop_triangles()
                    base = len(positions) // 3
                    transform = evaluated.matrix_world
                    for vertex in mesh.vertices:
                        coordinate = transform @ vertex.co
                        positions.extend((float(coordinate.x), float(coordinate.y), float(coordinate.z)))
                    for triangle in mesh.loop_triangles:
                        faces.extend(base + int(index) for index in triangle.vertices)
                finally:
                    evaluated.to_mesh_clear()
            if positions and faces:
                native.hamun_renderer_mesh_preview.argtypes = [
                    ctypes.c_uint32, ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                    ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                    ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                ]
                native.hamun_renderer_mesh_preview.restype = ctypes.c_int
                position_buffer = (ctypes.c_float * len(positions))(*positions)
                index_buffer = (ctypes.c_uint32 * len(faces))(*faces)
                if scene.camera is not None:
                    camera = scene.camera.evaluated_get(depsgraph)
                    projection = camera.calc_matrix_camera(
                        depsgraph, x=width, y=height,
                        scale_x=scene.render.pixel_aspect_x,
                        scale_y=scene.render.pixel_aspect_y)
                    view = camera.matrix_world.inverted_safe()
                    vp = projection @ view
                    camera_data = (ctypes.c_float * 16)(
                        *(vp[row][column] for column in range(4) for row in range(4)))
                    native.hamun_renderer_mesh_camera.argtypes = [
                        ctypes.c_uint32, ctypes.c_uint32,
                        ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint64,
                        ctypes.POINTER(ctypes.c_float), ctypes.c_uint32,
                        ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32,
                        ctypes.POINTER(ctypes.c_float),
                    ]
                    native.hamun_renderer_mesh_camera.restype = ctypes.c_int
                    ok = native.hamun_renderer_mesh_camera(
                        width, height, pixels, ctypes.sizeof(pixels),
                        position_buffer, len(positions) // 3,
                        index_buffer, len(faces), camera_data)
                else:
                    ok = native.hamun_renderer_mesh_preview(
                        width, height, pixels, ctypes.sizeof(pixels),
                        position_buffer, len(positions) // 3,
                        index_buffer, len(faces))
            else:
                ok = native.hamun_renderer_offscreen_clear(
                    width, height, pixels, ctypes.sizeof(pixels))
            if not ok:
                message = native.hamun_renderer_last_error()
                raise RuntimeError(message.decode("utf-8", "replace") if message else "Renderer preview failed")
            # Blender Render Result expects bottom-to-top float RGBA per pixel.
            rgba = [channel / 255.0 for channel in pixels]
            result = self.begin_result(0, 0, width, height)
            if not result:
                raise RuntimeError("Cannot create Blender Render Result")
            try:
                combined = result.layers[0].passes["Combined"]
                combined.rect.foreach_set(rgba)
            finally:
                self.end_result(result)
            self.report({"INFO"}, "Vulkan offscreen image transferred to Render Result")
        except Exception as exc:
            self.report({"ERROR"}, "Hamun Vulkan F12: " + str(exc))


class HAMUN_OT_probe_renderer(bpy.types.Operator):
    """Load HamunRenderer.dll directly into Editor and query Vulkan GPU"""
    bl_idname = "hamun.probe_renderer"
    bl_label = "Check Integrated Vulkan"
    bl_description = "Verify direct DLL integration without launching an external executable"

    def execute(self, context):
        import ctypes
        dll = _hamun_renderer_dll(context.scene)
        if not dll or not dll.is_file():
            self.report({"ERROR"}, "HamunRenderer.dll not found in build or Editor directory")
            return {"CANCELLED"}

        class RendererInfo(ctypes.Structure):
            _fields_ = [
                ("abiVersion", ctypes.c_uint32),
                ("vulkanVersion", ctypes.c_uint32),
                ("gpuVendor", ctypes.c_uint32),
                ("deviceName", ctypes.c_char * 256),
            ]
        try:
            renderer = ctypes.WinDLL(str(dll))
            renderer.hamun_renderer_probe.argtypes = [ctypes.POINTER(RendererInfo)]
            renderer.hamun_renderer_probe.restype = ctypes.c_int
            renderer.hamun_renderer_last_error.restype = ctypes.c_char_p
            info = RendererInfo()
            if not renderer.hamun_renderer_probe(ctypes.byref(info)):
                error = renderer.hamun_renderer_last_error()
                raise RuntimeError(error.decode("utf-8", "replace") if error else "Vulkan probe failed")
            context.scene["hamun_renderer_gpu"] = info.deviceName.decode("utf-8", "replace")
            self.report({"INFO"}, "HamunRenderer.dll: " + context.scene["hamun_renderer_gpu"])
        except Exception as exc:
            self.report({"ERROR"}, "HamunRenderer.dll: " + str(exc))
            return {"CANCELLED"}
        return {"FINISHED"}


class HAMUN_PT_vulkan_settings(bpy.types.Panel):
    """Settings for the external Hamun Vulkan runtime (preview integration)."""
    bl_label = "Hamun Vulkan Runtime"
    bl_idname = "HAMUN_PT_vulkan_settings"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        return context.engine == "HAMUN_VULKAN"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.label(text="External Vulkan runtime preview", icon="INFO")
        layout.label(text="F12: in-editor render preview enabled")
        dll = _hamun_renderer_dll(scene)
        layout.label(text="Renderer: " + ("DLL found" if dll else "DLL missing"), icon="CHECKMARK" if dll else "ERROR")
        layout.prop(scene, "hamun_vulkan_fps_cap")
        shadow_box = layout.box()
        shadow_box.label(text="Shadows", icon="SHADING_RENDERED")
        shadow_box.prop(scene, "hamun_shadow_map_resolution", text="Shadow Map")
        layout.operator("hamun.probe_renderer", text="Check Integrated Vulkan", icon="SHADING_RENDERED")
        layout.operator("hamun.diagnose_viewport", text="Write Diagnostic Log", icon="TEXT")
        if scene.get("hamun_renderer_gpu"):
            layout.label(text="GPU: " + scene["hamun_renderer_gpu"])
        layout.separator()
        button = layout.operator("hamun.play_runtime", text="Play Scene in Vulkan", icon="PLAY")
        button.runtime_exe = scene.hamun_vulkan_executable


class HAMUN_PT_authoring(bpy.types.Panel):
    bl_label = "Hamun Engine"
    bl_idname = "HAMUN_PT_authoring"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Hamun"

    def draw(self, context):
        layout = self.layout
        layout.operator("hamun.create_project", icon="FILE_NEW")
        layout.operator("hamun.import_asset", icon="IMPORT")
        layout.operator("hamun.save_project", icon="FILE_TICK")
        layout.operator("hamun.open_project", icon="FILE_FOLDER")
        layout.operator("hamun.recent_projects", icon="RECOVER_LAST")
        layout.operator("hamun.save_project_as", icon="DUPLICATE")
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
        layout.operator("hamun.play_runtime", text="Play in Hamun Runtime", icon="PLAY")
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
    HAMUN_OT_import_asset,
    HAMUN_OT_save_project,
    HAMUN_OT_quick_save,
    HAMUN_OT_save_project_as,
    HAMUN_OT_recent_projects,
    HAMUN_OT_open_project,
    HAMUN_OT_upgrade_project,
    HAMUN_OT_prepare_scene,
    HAMUN_OT_setup_workspace,
    HAMUN_OT_export_scene,
    HAMUN_OT_play_runtime,
    HAMUN_OT_probe_renderer,
    HAMUN_OT_toggle_metrics,
    HAMUN_OT_set_metrics_mode,
    HAMUN_OT_diagnose_viewport,
    HAMUN_RENDER_VULKAN,
    HAMUN_PT_vulkan_settings,
    HAMUN_OT_link_glb,
    HAMUN_OT_export_hscene,
    HAMUN_OT_import_hscene,
    HAMUN_PT_authoring,
    HAMUN_MT_transform_menu,
)


def _hamun_light_panel_compat(enable):
    """Expose Blender's native light properties for our custom render engine."""
    from bl_ui import properties_data_light
    for panel in properties_data_light.classes:
        compatible = getattr(panel, "COMPAT_ENGINES", None)
        if compatible is not None:
            if enable:
                compatible.add("HAMUN_VULKAN")
            else:
                compatible.discard("HAMUN_VULKAN")


def register():
    _hamun_light_panel_compat(True)
    bpy.types.Scene.hamun_vulkan_executable = bpy.props.StringProperty(
        name="Runtime Executable", subtype="FILE_PATH",
        description="Path to HamunRuntime.exe"
    )
    bpy.types.Scene.hamun_shadow_map_resolution = bpy.props.EnumProperty(
        name="Shadow Map Resolution",
        description="CPU Sun shadow resolution (higher quality costs CPU time and memory)",
        items=[(str(i), f"{i} x {i}", f"Sun shadow resolution: {i} x {i}")
               for i in (64, 128, 256, 512, 1024, 2048, 4096)],
        default="512",
    )
    bpy.types.Scene.hamun_vulkan_fps_cap = bpy.props.EnumProperty(
        name="Target FPS", description="Informational setting; runtime key F6 currently sets cap",
        items=[('0','Unlimited',''),('30','30',''),('60','60',''),('120','120',''),('144','144','')],
        default='0'
    )
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.VIEW3D_MT_editor_menus.append(draw_hamun_menu)
    bpy.types.TOPBAR_MT_file.prepend(draw_hamun_file_menu)
    _register_hamun_shortcuts()
    if not bpy.app.timers.is_registered(_hamun_metrics_refresh_timer):
        bpy.app.timers.register(_hamun_metrics_refresh_timer, first_interval=0.25)


def unregister():
    _hamun_light_panel_compat(False)
    _hamun_metrics.pop("active_engines", None)
    if bpy.app.timers.is_registered(_hamun_metrics_refresh_timer):
        bpy.app.timers.unregister(_hamun_metrics_refresh_timer)
    _unregister_hamun_shortcuts()
    bpy.types.TOPBAR_MT_file.remove(draw_hamun_file_menu)
    bpy.types.VIEW3D_MT_editor_menus.remove(draw_hamun_menu)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.hamun_shadow_map_resolution
    del bpy.types.Scene.hamun_vulkan_fps_cap
    del bpy.types.Scene.hamun_vulkan_executable


if __name__ == "__main__":
    register()
