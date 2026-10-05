# HamunEditor workflow

HamunEditor is the first usable scene-authoring workflow in BDFR Hamun Engine.

## Start a project

1. Launch `HamunLauncher.exe`.
2. Create a **Blank Project**.
3. The generated project opens in HamunEditor when the editor executable is available.
4. New projects contain:
   - `Project.hamunproject`
   - `Assets/Starter/TestScene.gltf`
   - `Assets/Starter/TestScene.bin`
   - `Assets/Starter/checker.png`
   - `Scenes/Main.hamunscene`

The starter content is project-owned, so edits and scene documents do not depend on the engine source tree.

## Scene workflow

HamunEditor opens `.hamunscene`, `.gltf`, and `.glb` scene sources.

A `.hamunscene` document stores:
- source glTF asset
- scene object structure
- object names and hierarchy depth
- Position / Rotation / Scale
- per-object Base Color / Metallic / Roughness overrides
- editor camera pose

Older transform-only scene documents remain readable.

Use:
- **Ctrl+S** — Save Scene
- **File > Save Scene As** — save another scene document
- **Ctrl+Z / Ctrl+Y** — undo/redo transform edits
- **Ctrl+D** — duplicate selected object
- **Delete** — delete selected object/subtree
- **F** — focus selected object in the viewport
- **Home** — reset editor camera

Unsaved scene edits are protected by a Save / Discard / Cancel prompt when closing or switching project/scene.

## Viewport

The viewport renders through HamunRenderer and HamunRHI.

Renderer menu:
- Auto (DX12 -> DX11)
- DirectX 12
- DirectX 11

Navigation:
- **RMB drag** — look
- **W/A/S/D** — move
- **Q/E** — down/up
- **Shift** — sprint
- **Mouse wheel** — forward/back
- **Left click** — pick a mesh/entity
- **F** — focus Outliner selection
- **Home** — reset camera

The viewport header reports renderer, adapter, resolution, camera position, FPS, and frame time.

## Outliner and Inspector

The Outliner is populated from the loaded scene and backed by HamunWorld entities.

Inspector authoring currently supports:
- Rename object
- Position X/Y/Z
- Rotation X/Y/Z in degrees
- Scale X/Y/Z
- Base Color RGB
- Metallic
- Roughness
- Entity ID / parent Entity visibility

Changes are reflected immediately in the live viewport.

## Asset Browser

The Asset Browser recursively indexes project content and classifies:
- Hamun scenes
- glTF / GLB
- textures
- TressFX hair assets
- C/C++ source
- project files
- other files

Use:
- **F5** — refresh project assets
- **File > Import Asset** — import into `Assets/Imported/<asset-name>/`
- **File > Open Project Folder** — open the project directory
- double-click a scene or glTF asset to open it
- drag supported files onto HamunEditor

For `.gltf` imports, common sibling `.bin` and image companions are copied with the source file.

## Current limitations

This is a pre-alpha authoring baseline, not a finished production editor.

Still planned:
- transform gizmos
- full glTF node graph fidelity, including transform-only nodes
- texture-slot/material graph editing
- multi-asset scene composition beyond a single source glTF
- Play-in-Editor/gameplay runtime integration
- HamunGraph editor
- terrain/world-partition tools
- profiler/debugger UI
