# Native Hamun projects (.hamun)

The Hamun Editor now writes `ProjectName.hamun` (UTF-8 JSON) in the project directory. This is a **project metadata document**, not a renamed Blender file or a playable runtime scene.

Its `startup_scene` is a relative path to `Scenes/Main.blend`, which is still editable in the Blender-derived editor.

## Install the new editor tools locally

Close HamunEditor, then in the Visual Studio x64 Native Tools Command Prompt run:

```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\blender.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

Look for `HAMUN_TOOLS_INSTALLED`, then open HamunEditor normally. Press **N** in the 3D View and open the **Hamun** tab.

- **Create Hamun Project** creates `ProjectName.hamun`, `Scenes/Main.blend` and the Assets/Scenes/Materials/Scripts folders.
- **Open Hamun Project** reads a `.hamun` or `hamun.project.json` file and opens its source scene. Save current work before opening another project.
- **Convert Legacy Project to .hamun** creates a `.hamun` manifest for an existing project, without deleting or changing its old `hamun.project.json` and Blender scenes.

For your existing `D:\HamunProjects\MyHamunProject`, choose **Convert Legacy Project to .hamun**, then select `D:\HamunProjects\MyHamunProject\hamun.project.json`. The new file will be `MyHamunProject.hamun`.

No C++ rebuild is necessary: these are editor tools loaded by the existing HamunEditor build.

**Not yet implemented:** a native `.hscene` runtime serialization format, asset dependency indexing, gameplay scene loading, and automatic `.hamun` file association on Windows. Existing .blend scenes must not be renamed to .hscene.
