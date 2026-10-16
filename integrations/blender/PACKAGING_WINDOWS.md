# Windows x64 portable packaging

This procedure packages an **already compiled and locally tested** Blender-based
Hamun Editor. It does not rebuild Blender or publish a GitHub Release.

## Prerequisites

- Windows 11 x64 and PowerShell 5.1 or newer.
- A complete Blender Release folder from the pinned Blender base
  `2622ed297d669e71dbfdc3478d844ed6004d9285`, with the Hamun branding
  source changes applied and built.
- The Release folder must contain `HamunEditor.exe`, `blender.exe`,
  `5.4/scripts`, `license/license.md`, and `license/licenses.json`.
- This repository checked out on `feature/blender-source-integration`.

The packager copies `integrations/blender/hamun_authoring_tools` into
`5.4/scripts/addons/hamun_authoring_tools` and
`integrations/blender/startup/hamun_startup.py` into `5.4/scripts/startup`.

## Package

From `D:\HamunDev\Hamun`:

```bat
git pull --ff-only origin feature/blender-source-integration
powershell -NoProfile -ExecutionPolicy Bypass -File "D:\HamunDev\Hamun\integrations\blender\package_windows.ps1"
```

Override build or output location as needed:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File "D:\HamunDev\Hamun\integrations\blender\package_windows.ps1" -ReleaseDir "D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release" -OutputDir "D:\HamunDev\HamunPackages"
```

Output (default):

- `D:\HamunDev\HamunPackages\HamunEditor-v0.1.0-alpha-Windows-x64\`
- `D:\HamunDev\HamunPackages\HamunEditor-v0.1.0-alpha-Windows-x64.zip`
- `D:\HamunDev\HamunPackages\HamunEditor-v0.1.0-alpha-Windows-x64.sha256.txt`

## Test

Extract the ZIP into a **new** folder, then launch
`HamunEditor.exe`. Verify the Hamun menu and sidebar are present, project
creation/save/open works, and GLB / `.hscene` operations work.

Test on a separate Windows machine without Blender or development tools.
The local clean-profile test is useful but not a substitute.

## Before a public release

- Review the complete GPL and third-party license obligations and provide the
  corresponding source, including modifications, as applicable.
- Check build instructions, Blender base revision, patch order and restore
  procedure (the existing `0001` and `0002` patches must not be applied twice).
- Confirm clean-machine testing, release notes, SHA256, and antivirus scanning.
- Upload the tested ZIP manually or through a verified release workflow.

**No public distribution is authorized by merely running this script.**
