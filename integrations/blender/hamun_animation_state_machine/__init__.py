bl_info = {
    "name": "Hamun Animation State Machine",
    "author": "Hamun Engine",
    "version": (0, 1, 0),
    "blender": (4, 3, 0),
    "location": "Node Editor > Hamun Animation",
    "description": "Node-based animation state machine authoring and JSON export for Hamun",
    "category": "Animation",
}

import bpy
import json
from pathlib import Path
from bpy.props import BoolProperty, EnumProperty, FloatProperty, PointerProperty, StringProperty


class HAMUN_ASM_Socket(bpy.types.NodeSocket):
    bl_idname = "HAMUN_ASM_Socket"
    bl_label = "State Flow"

    def draw(self, context, layout, node, text):
        layout.label(text=text)

    def draw_color(self, context, node):
        return (0.25, 0.7, 0.95, 1.0)


class HAMUN_ASM_Tree(bpy.types.NodeTree):
    bl_idname = "HAMUN_ASM_Tree"
    bl_label = "Hamun Animation"
    bl_icon = "ARMATURE"


class HAMUN_ASM_Entry(bpy.types.Node):
    bl_idname = "HAMUN_ASM_Entry"
    bl_label = "Entry"

    def init(self, context):
        self.outputs.new("HAMUN_ASM_Socket", "Start")
        self.width = 140


class HAMUN_ASM_State(bpy.types.Node):
    bl_idname = "HAMUN_ASM_State"
    bl_label = "Animation State"

    action_name: StringProperty(name="Action")
    looping: BoolProperty(name="Loop", default=True)
    playback_speed: FloatProperty(name="Speed", default=1.0, min=0.01, max=10.0)

    def init(self, context):
        self.inputs.new("HAMUN_ASM_Socket", "In")
        self.outputs.new("HAMUN_ASM_Socket", "Out")
        self.width = 190

    def draw_buttons(self, context, layout):
        layout.prop_search(self, "action_name", bpy.data, "actions", text="Action")
        layout.prop(self, "looping")
        layout.prop(self, "playback_speed")


class HAMUN_ASM_Transition(bpy.types.Node):
    bl_idname = "HAMUN_ASM_Transition"
    bl_label = "Transition"

    parameter: StringProperty(name="Parameter", default="Speed")
    op: EnumProperty(name="Compare", items=[
        ("GT", ">", ""), ("GE", ">=", ""), ("LT", "<", ""),
        ("LE", "<=", ""), ("EQ", "==", ""), ("NE", "!=", ""),
    ], default="GT")
    threshold: FloatProperty(name="Value", default=0.1)
    blend_seconds: FloatProperty(name="Blend (s)", default=0.2, min=0.0, max=30.0)
    priority: FloatProperty(name="Priority", default=0.0)

    def init(self, context):
        self.inputs.new("HAMUN_ASM_Socket", "From")
        self.outputs.new("HAMUN_ASM_Socket", "To")
        self.width = 175

    def draw_buttons(self, context, layout):
        layout.prop(self, "parameter")
        row = layout.row(align=True)
        row.prop(self, "op", text="")
        row.prop(self, "threshold", text="")
        layout.prop(self, "blend_seconds")
        layout.prop(self, "priority")


class HAMUN_ASM_Category(bpy.types.NodeCategory if hasattr(bpy.types, "NodeCategory") else object):
    pass


def machine_to_dict(tree):
    """Validate graph and serialize without relying on mutable Blender node names."""
    entries = [n for n in tree.nodes if n.bl_idname == HAMUN_ASM_Entry.bl_idname]
    states = [n for n in tree.nodes if n.bl_idname == HAMUN_ASM_State.bl_idname]
    transitions = [n for n in tree.nodes if n.bl_idname == HAMUN_ASM_Transition.bl_idname]
    if len(entries) != 1:
        raise ValueError("Exactly one Entry node is required")
    if not states:
        raise ValueError("Add at least one Animation State")
    ids = {n: "state_%03d" % i for i, n in enumerate(sorted(states, key=lambda n: n.name), 1)}

    def links_from(node):
        return [link for link in tree.links if link.from_node == node]

    def links_to(node):
        return [link for link in tree.links if link.to_node == node]

    entry_links = links_from(entries[0])
    if len(entry_links) != 1 or entry_links[0].to_node not in ids:
        raise ValueError("Entry must connect directly to exactly one State")
    result = {
        "format": "hamun.animation.state_machine",
        "version": 1,
        "name": tree.name,
        "entry": ids[entry_links[0].to_node],
        "states": [
            {"id": ids[n], "name": n.name, "action": n.action_name,
             "loop": n.looping, "speed": n.playback_speed}
            for n in sorted(states, key=lambda n: n.name)
        ],
        "transitions": [],
    }
    for n in transitions:
        incoming = links_to(n)
        outgoing = links_from(n)
        if len(incoming) != 1 or len(outgoing) != 1:
            raise ValueError("Transition '%s' needs exactly one input and one output" % n.name)
        source, target = incoming[0].from_node, outgoing[0].to_node
        if source not in ids or target not in ids:
            raise ValueError("Transition '%s' must connect State -> Transition -> State" % n.name)
        if not n.parameter.strip():
            raise ValueError("Transition '%s' has an empty parameter" % n.name)
        result["transitions"].append({
            "from": ids[source], "to": ids[target],
            "condition": {"parameter": n.parameter.strip(), "operator": n.op, "value": n.threshold},
            "blend_seconds": n.blend_seconds, "priority": n.priority,
        })
    result["transitions"].sort(key=lambda t: (t["from"], -t["priority"], t["to"]))
    return result


class HAMUN_ASM_OT_Sample(bpy.types.Operator):
    bl_idname = "hamun_asm.sample"
    bl_label = "Create Idle / Walk Demo"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        tree = bpy.data.node_groups.new("Hamun Locomotion", HAMUN_ASM_Tree.bl_idname)
        entry = tree.nodes.new(HAMUN_ASM_Entry.bl_idname)
        entry.location = (-420, 120)
        idle = tree.nodes.new(HAMUN_ASM_State.bl_idname)
        idle.name = "Idle"
        idle.location = (-200, 150)
        walk = tree.nodes.new(HAMUN_ASM_State.bl_idname)
        walk.name = "Walk"
        walk.location = (260, 150)
        go = tree.nodes.new(HAMUN_ASM_Transition.bl_idname)
        go.name = "Start Moving"
        go.location = (30, 160)
        go.parameter, go.op, go.threshold = "Speed", "GT", 0.1
        stop = tree.nodes.new(HAMUN_ASM_Transition.bl_idname)
        stop.name = "Stop Moving"
        stop.location = (30, -160)
        stop.parameter, stop.op, stop.threshold = "Speed", "LE", 0.1
        tree.links.new(entry.outputs[0], idle.inputs[0])
        tree.links.new(idle.outputs[0], go.inputs[0])
        tree.links.new(go.outputs[0], walk.inputs[0])
        tree.links.new(walk.outputs[0], stop.inputs[0])
        tree.links.new(stop.outputs[0], idle.inputs[0])
        context.scene.hamun_asm_graph = tree
        if context.area and context.area.type == "NODE_EDITOR":
            context.space_data.tree_type = HAMUN_ASM_Tree.bl_idname
            context.space_data.node_tree = tree
        self.report({"INFO"}, "Hamun Locomotion graph created")
        return {"FINISHED"}


class HAMUN_ASM_OT_Export(bpy.types.Operator):
    bl_idname = "hamun_asm.export"
    bl_label = "Export Hamun State Machine"
    filepath: StringProperty(subtype="FILE_PATH", default="//hamun_animation.json")
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def execute(self, context):
        tree = context.scene.hamun_asm_graph
        if not tree:
            self.report({"ERROR"}, "Select a Hamun graph first")
            return {"CANCELLED"}
        try:
            data = machine_to_dict(tree)
            path = Path(bpy.path.abspath(self.filepath))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        except (ValueError, OSError) as exc:
            self.report({"ERROR"}, str(exc))
            return {"CANCELLED"}
        self.report({"INFO"}, "Exported %s" % path)
        return {"FINISHED"}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class HAMUN_ASM_PT_Panel(bpy.types.Panel):
    bl_label = "Hamun Animation State Machine"
    bl_idname = "HAMUN_ASM_PT_panel"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "Hamun"

    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == HAMUN_ASM_Tree.bl_idname

    def draw(self, context):
        col = self.layout.column(align=True)
        col.prop(context.scene, "hamun_asm_graph", text="Graph")
        col.operator("hamun_asm.sample", icon="ADD")
        col.operator("hamun_asm.export", icon="EXPORT")
        col.label(text="Add nodes: Shift+A > Search")


classes = (
    HAMUN_ASM_Socket, HAMUN_ASM_Tree, HAMUN_ASM_Entry,
    HAMUN_ASM_State, HAMUN_ASM_Transition,
    HAMUN_ASM_OT_Sample, HAMUN_ASM_OT_Export, HAMUN_ASM_PT_Panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.hamun_asm_graph = PointerProperty(type=HAMUN_ASM_Tree)


def unregister():
    del bpy.types.Scene.hamun_asm_graph
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
