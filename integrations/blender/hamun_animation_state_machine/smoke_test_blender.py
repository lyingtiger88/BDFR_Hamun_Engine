"""Smoke test the addon in Blender: blender -b --factory-startup --python smoke_test_blender.py

Run this file from its containing directory so the package can be imported.
"""
import json
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent))
import hamun_animation_state_machine as asm


def main():
    asm.register()
    tree = bpy.data.node_groups.new("Smoke Test", asm.HAMUN_ASM_Tree.bl_idname)
    entry = tree.nodes.new(asm.HAMUN_ASM_Entry.bl_idname)
    idle = tree.nodes.new(asm.HAMUN_ASM_State.bl_idname)
    walk = tree.nodes.new(asm.HAMUN_ASM_State.bl_idname)
    transition = tree.nodes.new(asm.HAMUN_ASM_Transition.bl_idname)
    idle.name = "Idle"
    idle.action_name = "IdleAction"
    walk.name = "Walk"
    walk.action_name = "WalkAction"
    tree.links.new(entry.outputs[0], idle.inputs[0])
    tree.links.new(idle.outputs[0], transition.inputs[0])
    tree.links.new(transition.outputs[0], walk.inputs[0])
    data = asm.machine_to_dict(tree)
    assert data["format"] == "hamun.animation.state_machine"
    assert len(data["states"]) == 2
    assert len(data["transitions"]) == 1
    print("HAMUN_BLENDER_SMOKE_OK", json.dumps(data))
    bpy.data.node_groups.remove(tree)
    asm.unregister()


if __name__ == "__main__":
    main()
