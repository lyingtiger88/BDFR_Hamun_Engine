# Hamun Vegetation Studio — v0.1 prototype

Experimental procedural tree authoring module for the Blender-based Hamun editor.

## Scope

- Viewport N-panel: **Hamun Vegetation**
- Editable height, branch levels, leaves per tip, deterministic random seed
- Generates separate bark/leaf meshes and basic Principled BSDF materials
- Non-destructive to existing meshes; new collection for each generation
- Geometry can be exported using Blender's File > Export > glTF 2.0

**Not yet implemented:** MTree integration, leaf UVs/textures, biologically realistic growth, LOD, wind, collisions, automatic GLB export, managed regeneration, shrub generator, asset library. This is a technical smoke-test, not a production-ready SpeedTree equivalent.

## Compatibility

Uses Blender bpy API with minimum version 4.3 declared. Blender 5.4 Alpha in Hamun has **not yet been tested**. Do not package into production releases before a local smoke test.

## Install / Run

1. Clone branch `feature/hamun-vegetation-studio-v0.1`.
2. Zip the **hamun_vegetation_studio** folder (its `__init__.py` must be at folder root within zip) or install it as a development add-on through your existing authoring-tools packaging routine.
3. Launch Hamun editor and use Edit > Preferences > Add-ons > Install from Disk, select the zip, enable the add-on.
4. In 3D Viewport, press N > **Hamun Vegetation**.
5. Click **Generate Tree**.
6. Check Outliner for HVS_Tree_0042, HVS_Bark and HVS_Leaves; modify seed and regenerate.
7. Optionally select generated objects and use File > Export > glTF 2.0 (.glb).

If the custom alpha build does not expose Blender add-on installation, integrate through its existing Python script loading pathway and call `register()` once. Never copy into the Blender installation's bundled system scripts until tested.

## Acceptance test

- Open empty scene; generate twice with same seed; verify consistent topology.
- Change seed/height; verify output changes.
- Undo creation; verify scene recovers.
- Save/reopen .blend; verify meshes/materials persist.
- Export selected objects to GLB and import into a fresh Blender scene.
- Collect startup console and Python traceback if the alpha build rejects registration.

## Next milestone: MTree compatibility spike

Source: https://github.com/GoodPie/modular_tree
Official Blender extension: https://extensions.blender.org/add-ons/modular-tree/

MTree Blender add-on is GPLv3; upstream C++ core is MIT. Assess licenses for source reuse and compiled-distribution separately, retaining upstream notices. Test the official Windows extension **5.5.2** with Hamun Blender 5.4 Alpha in an isolated development build. Do not embed or ship the GPL add-on as proprietary code.

Track: registration, binary module loading / Python ABI, node editor registration, basic oak preset generation, save/reopen, GLB export. Then decide whether to use MTree directly or a clean adapter around the MIT core.

## Isolation

This branch intentionally does not touch the Windows packaging scripts, bundled Blender runtime, or the user's local files. Merge only after the tests above pass.
