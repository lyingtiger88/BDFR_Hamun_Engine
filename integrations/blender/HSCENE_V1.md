# HamunScene (.hscene) v1

This is an experimental interoperable JSON scene schema, **not** a renamed `.blend` and not a runtime-ready mesh serialization format.

```json
{
  "format": "HamunScene",
  "schema_version": 1,
  "name": "Scene",
  "coordinate_system": "Blender_Z_UP",
  "transform_space": "world",
  "entities": [
    {
      "id": "Cube",
      "name": "Cube",
      "type": "MESH",
      "parent": null,
      "transform_space": "world",
      "position": [0.0, 0.0, 0.0],
      "rotation_quaternion": [1.0, 0.0, 0.0, 0.0],
      "scale": [1.0, 1.0, 1.0]
    }
  ]
}
```

IDs currently match the unique Blender object name and are **not permanent UUIDs**. Rename an object and its exported ID changes. The importer checks IDs, hierarchy, cycles, coordinate system, and finite transform values before adding objects.

- Export: N sidebar > Hamun > **Export Hamun Scene (.hscene)**. Save alongside `Scenes/Main.blend`, e.g. `Scenes/Main.hscene`.
- Import: N sidebar > Hamun > **Import Hamun Scene (.hscene)**. Imports into a new **Hamun Import** collection as **empty transform placeholders**. Geometry, materials, cameras, lights, and animation are **not rebuilt**; those need a separate asset pipeline.
- Import does not replace or delete the current Blender scene.
- `.hamun` is the project manifest format; `.hscene` contains scene entities. `.blend` remains the native authoring data file.

## Test on local Windows editor

Close HamunEditor. In the Visual Studio x64 command prompt:

```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\blender.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

After `HAMUN_TOOLS_INSTALLED`, restart `HamunEditor.exe`, open the existing project, export the `.hscene`, then import it in a new empty `.blend` file. Check the Outliner for imported placeholders and that transforms and parent relations are retained.

There is **no gameplay runtime reader** for this file yet.
