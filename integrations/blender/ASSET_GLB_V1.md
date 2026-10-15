# GLB assets in HamunScene v1 (experimental)

The editor can now reference a pre-existing .glb under the current project's Assets directory.

For the first supported case, use a GLB containing **exactly one mesh object**, authored in local origin coordinates. GLBs with multiple meshes, rigs or complex internal transforms are not yet supported by this importer. Other imported HScene entities remain Empty transform placeholders.

## Windows test

1. Close HamunEditor and update from the Visual Studio x64 command prompt:

```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\blender.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

2. Reopen HamunEditor and open `MyHamunProject.hamun`.
3. Place a single-mesh GLB file in `D:\HamunProjects\MyHamunProject\Assets\` (e.g. `Cube.glb`). When making one with Blender's glTF exporter, select only a mesh object and choose **Selected Objects**.
4. Select the matching mesh in the Blender scene. In N sidebar > Hamun choose **Link Project GLB**, then select the GLB from the project's Assets folder.
5. Choose **Export Hamun Scene (.hscene)** and write `Scenes/Main.hscene`.
6. Open a new Blender file, choose **Import Hamun Scene (.hscene)** and select `Scenes/Main.hscene`. Imported objects with valid GLB references should now have mesh geometry; objects without references remain placeholders.

A referenced asset path is stored in each HScene entity as `"asset": "Assets/Cube.glb"`. Import validates paths are inside the project's Assets folder to prevent absolute/traversal paths. Import geometry support is **limited** and may not preserve materials/complex transforms. Existing scenes and legacy HScene documents without `asset` remain readable.

This stage has not been verified on the user's local Windows build. It does not yet implement a runtime asset loader or automated import/export of multi-object geometry.
