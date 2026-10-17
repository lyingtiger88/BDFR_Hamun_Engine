"""Batch smoke test: HamunEditor.exe -b --python smoke_test_hvs.py -- <output_dir>"""
import bpy
import json
import os
import sys
import traceback
from pathlib import Path

def main():
    argv = sys.argv
    args = argv[argv.index("--") + 1:] if "--" in argv else []
    output = Path(args[0]).resolve() if args else Path.cwd() / "hvs_smoke_output"
    output.mkdir(parents=True, exist_ok=True)
    addon_root = Path(__file__).resolve().parent / "hamun_vegetation_studio"
    sys.path.insert(0, str(addon_root.parent))
    import hamun_vegetation_studio as hvs
    if not hasattr(bpy.types.Scene, "hvs_settings"):
        hvs.register()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    settings = bpy.context.scene.hvs_settings
    settings.height = 8.0
    settings.levels = 4
    settings.leaves_per_tip = 6
    settings.seed = 42
    col = hvs.create_tree(settings)
    objs = list(col.objects)
    assert len(objs) == 2, "Expected bark and leaf mesh objects"
    assert all(obj.type == "MESH" and len(obj.data.vertices) for obj in objs)
    assert all(obj.get("hamun_seed") == 42 for obj in objs)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objs:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    glb_path = output / "HVS_Tree_0042.glb"
    bpy.ops.export_scene.gltf(filepath=str(glb_path), export_format="GLB", use_selection=True)
    assert glb_path.exists() and glb_path.stat().st_size > 1000
    blend_path = output / "HVS_Tree_0042.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    result = {
        "status": "PASS",
        "blender": bpy.app.version_string,
        "glb": str(glb_path),
        "blend": str(blend_path),
        "objects": [{"name":o.name, "vertices":len(o.data.vertices), "faces":len(o.data.polygons)} for o in objs],
    }
    (output / "hvs_result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("HVS_SMOKE_TEST_PASS " + json.dumps(result))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        sys.exit(1)
