# Hamun project workflow (Windows local build)

This is the first **real authoring workflow**, added to the Blender-derived Hamun Editor. It does not yet make a Hamun runtime scene or gameplay project.

## Install into the existing locally built editor

From **x64 Native Tools Command Prompt for VS 2022**:

```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\HamunEditor.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

The installer uses Blender's current user-script folder for this Blender version and enables the addon in user preferences. Restart HamunEditor normally after installation.

In the **3D View**, press `N`, choose **Hamun**, then click **Create Hamun Project**.
Supply the desired project name and an existing parent directory (e.g. `D:\HamunProjects`). The project directory must not already exist.

Created tree:

```text
MyHamunProject/
  hamun.project.json
  Assets/
  Scenes/
    Main.blend
  Materials/
  Scripts/
```

The scene saved under `Scenes/Main.blend` is a Blender source scene. Use **Export Scene to Hamun (glTF)** to produce a `.glb` for later engine integration.

## Reinstall after changing addon code

Re-run the installer command, then restart the editor. No C++ rebuild required for Python-only authoring tools. If the command fails, capture its full output and stop; do not delete any local Blender build.

## Current limits

- The editor depends on Blender's compiled binaries, Python and asset files.
- The project manifest is not yet consumed by the Hamun runtime.
- Scenes are still Blender `.blend` files; do not mistake the manifest for a playable game.
- This package keeps the upstream Blender license and source obligations.
