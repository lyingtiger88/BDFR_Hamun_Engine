bl_info = {
    "name": "Hamun Vegetation Studio",
    "author": "Hamun Engine contributors",
    "version": (0, 1, 0),
    "blender": (4, 3, 0),
    "location": "View3D > Sidebar > Hamun Vegetation",
    "description": "Experimental procedural tree generator for Hamun",
    "category": "Add Mesh",
}

import bpy
import math
import random
from mathutils import Vector
from bpy.props import FloatProperty, IntProperty, PointerProperty


def ensure_material(name, color, roughness=0.85):
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.diffuse_color = (*color, 1.0)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
    return material


def branch_mesh(vertices, faces, start, end, r0, r1, sides=6):
    axis = (end - start).normalized()
    side = axis.cross(Vector((0, 1, 0)))
    if side.length < 0.0001:
        side = axis.cross(Vector((1, 0, 0)))
    side.normalize()
    other = axis.cross(side).normalized()
    base = len(vertices)
    for p, radius in ((start, r0), (end, r1)):
        for k in range(sides):
            direction = side * math.cos(k * 2 * math.pi / sides) + other * math.sin(k * 2 * math.pi / sides)
            vertices.append(tuple(p + direction * radius))
    for k in range(sides):
        j = (k + 1) % sides
        faces.append((base + k, base + j, base + sides + j, base + sides + k))
    faces.append(tuple(base + k for k in reversed(range(sides))))
    faces.append(tuple(base + sides + k for k in range(sides)))


def add_leaf(vertices, faces, center, direction, width, length):
    direction = direction.normalized()
    right = direction.cross(Vector((0, 0, 1)))
    if right.length < 0.001:
        right = Vector((1, 0, 0))
    right.normalize()
    base = len(vertices)
    mid = center + direction * (length * 0.48)
    tip = center + direction * length
    vertices.extend([
        tuple(center),
        tuple(mid + right * width * 0.5),
        tuple(tip),
        tuple(mid - right * width * 0.5),
    ])
    faces.extend(((base, base + 1, base + 2), (base, base + 2, base + 3)))


def create_tree(settings):
    rng = random.Random(settings.seed)
    bark_verts, bark_faces, leaf_verts, leaf_faces = [], [], [], []
    height = settings.height
    max_levels = settings.levels
    segments = 0
    max_segments = 2500

    def grow(start, direction, length, radius, level):
        nonlocal segments
        if segments >= max_segments or length < 0.065:
            return
        segments += 1
        end = start + direction.normalized() * length
        branch_mesh(bark_verts, bark_faces, start, end, radius, max(radius * 0.72, 0.006))
        if level >= max_levels:
            for _ in range(settings.leaves_per_tip):
                outward = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.15, 1.0)))
                if outward.length < 0.01:
                    outward = Vector((1, 0, 0))
                add_leaf(leaf_verts, leaf_faces, end, outward, height * 0.035, height * 0.085)
            return
        children = 3 if level == 0 else 2
        for index in range(children):
            angle = 2 * math.pi * ((index / children) + rng.uniform(-0.11, 0.11))
            spread = 0.12 if level == 0 else 0.65
            next_dir = direction * (1.0 - spread) + Vector((
                math.cos(angle) * spread,
                math.sin(angle) * spread,
                rng.uniform(0.12, 0.34),
            ))
            grow(end, next_dir, length * rng.uniform(0.60, 0.78),
                 radius * 0.67, level + 1)
        if level < max_levels - 1:
            angle = rng.uniform(0, math.tau)
            lateral = Vector((math.cos(angle), math.sin(angle), 0.52)).normalized()
            grow(start.lerp(end, 0.65), lateral,
                 length * 0.55, radius * 0.52, level + 1)

    grow(Vector((0, 0, 0)), Vector((0, 0, 1)), height * 0.37,
         height * 0.032, 0)
    collection = bpy.data.collections.new("HVS_Tree_%04d" % settings.seed)
    bpy.context.scene.collection.children.link(collection)

    for name, verts, faces, material in (
        ("HVS_Bark", bark_verts, bark_faces, ensure_material("HVS_Bark_Mat", (0.22, 0.12, 0.065))),
        ("HVS_Leaves", leaf_verts, leaf_faces, ensure_material("HVS_Leaf_Mat", (0.14, 0.36, 0.075))),
    ):
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        obj.data.materials.append(material)
        obj["hamun_vegetation_version"] = "0.1.0"
        obj["hamun_seed"] = settings.seed
        for poly in mesh.polygons:
            poly.use_smooth = True
    return collection


class HVSSettings(bpy.types.PropertyGroup):
    height: FloatProperty(name="Height", default=8.0, min=1, max=40, unit="LENGTH")
    levels: IntProperty(name="Branch levels", default=4, min=1, max=6)
    leaves_per_tip: IntProperty(name="Leaves per tip", default=6, min=0, max=20)
    seed: IntProperty(name="Seed", default=42, min=0, max=999999)


class HVS_OT_generate(bpy.types.Operator):
    bl_idname = "hvs.generate_tree"
    bl_label = "Generate Tree"
    bl_description = "Generate a procedural tree in a new collection"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        try:
            collection = create_tree(context.scene.hvs_settings)
        except Exception as exc:
            self.report({"ERROR"}, "HVS generation failed: %s" % exc)
            return {"CANCELLED"}
        self.report({"INFO"}, "Generated %s" % collection.name)
        return {"FINISHED"}


class HVS_PT_main(bpy.types.Panel):
    bl_label = "Hamun Vegetation Studio"
    bl_idname = "HVS_PT_main"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Hamun Vegetation"

    def draw(self, context):
        layout = self.layout
        props = context.scene.hvs_settings
        layout.prop(props, "height")
        layout.prop(props, "levels")
        layout.prop(props, "leaves_per_tip")
        layout.prop(props, "seed")
        layout.operator("hvs.generate_tree", icon="OUTLINER_OB_CURVES")
        layout.label(text="Prototype: static mesh, no wind/LOD")


classes = (HVSSettings, HVS_OT_generate, HVS_PT_main)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.hvs_settings = PointerProperty(type=HVSSettings)


def unregister():
    del bpy.types.Scene.hvs_settings
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
