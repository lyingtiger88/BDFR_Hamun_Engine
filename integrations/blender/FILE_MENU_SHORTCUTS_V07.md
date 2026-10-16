# Hamun File menu and keyboard shortcuts (v0.7)

This update adds Hamun project commands to the top of the existing **File** menu, while preserving Blender's original File menu as a fallback for source-file operations.

When the add-on is active:
- **Ctrl+S**: Save Hamun project (.blend and .hscene together) when an open scene belongs to a Hamun project; otherwise use the native Blender Save behavior.
- **Ctrl+O**: Open the Hamun project file picker (.hamun or legacy hamun.project.json).
- **File > New Hamun Project**, **Open Hamun Project**, **Save Hamun Project**, and **Import Asset** are added.
- Existing **Open**, **Save**, and **Save As** for Blender native files are still accessible in File menu. This is an integration step, not a replacement of all upstream menu functionality.

To install:
```bat
cd /d D:\HamunDev\Hamun
git pull origin feature/blender-source-integration
"D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release\blender.exe" --background --factory-startup --python "D:\HamunDev\Hamun\integrations\blender\install_hamun_tools.py"
```

Restart HamunEditor to activate menus/keymaps. For the first test, open an existing Hamun project and add a cube; press **Ctrl+S**, verify `Scenes/Main.blend` and `Scenes/Main.hscene` are updated. Press **Ctrl+O** and check that the file browser offers `.hamun`. Save and reopen your scene to ensure data is preserved.

**Not implemented yet:** integrated Hamun Save As, persistent file association, drag-and-drop asset import, and runtime Play.
