# Hamun simplified project workflow (v0.6, Windows)

Close HamunEditor, update and reinstall the embedded tools:

```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\blender.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

Restart HamunEditor. Press **N** in the 3D viewport and select the **Hamun** tab.

1. **Open Hamun Project** → choose `D:\HamunProjects\MyHamunProject\MyHamunProject.hamun`.
2. **Import Asset** → choose any **single-mesh .glb** file. The editor copies it into the project's Assets folder, imports it into the current scene, and stores the asset reference automatically. Existing filenames are not overwritten: collisions get a distinct suffix.
3. **Save Hamun Project** → saves the current `.blend` source and writes a matching `.hscene` beside it in Scenes. Existing non-GLB Blender objects still save completely to `.blend`, but their HScene runtime representation is only a transform placeholder.
4. To restore the editable project, use **Open Hamun Project** again. The actual mesh and other Blender features restore from `Main.blend`. There is no need to import HScene for ordinary editing.

**Current technical limitations:** Manual Blender Ctrl+S does not trigger Hamun HScene export; use **Save Hamun Project** for synchronized exports. Drag/drop automation and a Hamun runtime game preview are not yet implemented. Imported multi-mesh GLBs can be edited in Blender but cannot be automatically restored from HScene v1. Only the compiled local Blender-derived editor has been runtime-tested by the user; these new v0.6 commands need user testing.
