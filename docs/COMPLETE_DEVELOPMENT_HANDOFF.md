# COMPLETE HANDOFF — HAMUN ENGINE
**Purpose:** Transfer the ongoing Hamun Engine development work to a new ChatGPT conversation without losing project scope, decisions, current repository state, or next actions.

## 1. Project identity and scope

Project: **Hamun Engine**  
GitHub repository: https://github.com/lyingtiger88/BDFR_Hamun_Engine  
Current working branch for this work: `feature/blender-source-integration`

The goal is to develop a Hamun-branded editor/engine experience based on Blender source integration, with a recognizable but distinct identity. This work is about the **engine/editor showcase**, not gameplay map design.

The original concept:
- Use Blender's source code and mature editor workflows as the foundation.
- Create a Hamun-branded experience through carefully selected interface changes and custom visual identity.
- Make Transform tools (Move / Rotate / Scale) visually distinctive, especially their gizmos.
- Preserve useful Blender workflows rather than rebuilding every editor panel from scratch.
- Progress toward a real, runnable showcase build, not just concept art or design documents.

Do not mix this project with the user's other projects unless the user explicitly asks.

## 2. User collaboration requirements

- User prefers Persian (Farsi) responses.
- Explain technical steps clearly and in beginner-friendly language.
- When asking the user to perform local work, specify exact paths, commands, expected results, and what output to send back.
- Do not tease the user while debugging.
- Do not claim to have built, tested, run, or packaged anything unless the tools actually performed and verified that work.
- Keep these statuses distinct: planned, committed, tested, blocked, and not tested.
- The user wants active development and useful progress, not repeated vague promises.
- Do not ask unnecessary questions when a reasonable next step is clear.
- The assistant cannot execute commands directly on the user's Windows PC. It can prepare scripts and repository changes, but must be honest about what still needs to be run locally.

## 3. Confirmed repository state

Repository: `lyingtiger88/BDFR_Hamun_Engine`

Branch: `feature/blender-source-integration`

### Existing Blender transform-gizmo patch
Path:
`integrations/blender/patches/0001-hamun-transform-gizmo-colors.patch`

README:
`integrations/blender/patches/README.md`

Patch / README links:
- https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/integrations/blender/patches/0001-hamun-transform-gizmo-colors.patch
- https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/integrations/blender/patches/README.md

The patch proposes the following transform-axis colors:
- X: turquoise
- Y: orange
- Z: violet

The patch README identifies Blender source baseline commit:
`2622ed297d669e71dbfdc3478d844ed6004d9285`

**Important:** The patch has NOT been confirmed to apply cleanly to a full Blender checkout, and no patched build has been confirmed to compile or launch. Inspect the patch and its context before relying on it.

### Engine showcase handoff (created and committed)
File:
`docs/ENGINE_SHOWCASE_HANDOFF.md`

URL:
https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/docs/ENGINE_SHOWCASE_HANDOFF.md

Commit SHA:
`c8d8fa77c63bac4ccd956e93ee5b4f33d3485805`

This document defines the showcase scope, visual direction, staged implementation, acceptance criteria, and honest status-reporting rules.

### Earlier map-design document (not the current goal)
File:
`docs/MAP_DESIGN_HANDOFF.md`

URL:
https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/docs/MAP_DESIGN_HANDOFF.md

Commit SHA:
`b96406c834cdf745adfb349ef7db2b04a8eebdd1`

This document contains design concepts for three game environment maps (Red Basin, Copperline Industrial, Glass Delta). It is **not** the engine showcase and is not a list of implemented engine features. Do not continue map design unless the user explicitly changes the scope.

## 4. Current truthful status

### Committed to GitHub
- Initial Blender transform-gizmo color patch and README.
- `docs/MAP_DESIGN_HANDOFF.md` (an earlier, mistakenly interpreted map-design handoff).
- `docs/ENGINE_SHOWCASE_HANDOFF.md` (the correct engine showcase handoff).

### Not verified / not yet completed
- Patch application to a full Blender source checkout.
- A clean unmodified Blender baseline build.
- A patched Blender/Hamun build.
- Launching and testing the patched executable.
- A real `Hamun_Showcase.blend` scene.
- A working Windows build script such as `build_windows.bat`.
- A packaged runnable Hamun executable.
- A screen recording or screenshots captured from an actual running patched build.

Do not claim any of the above exists or has passed testing until verified.

## 5. Showcase target

The first deliverable should be a small, reproducible editor showcase, not a complete game engine rewrite.

The showcase should demonstrate:
1. The editor launches from a verified build.
2. A dedicated `Hamun_Showcase.blend` scene opens.
3. A small studio/test scene includes a floor, several primitive meshes, one test imported asset, a camera, and basic lighting.
4. Objects can be selected and transformed with Move, Rotate, and Scale.
5. Hamun's axis colors and selection/hover states are visible.
6. Viewport navigation works.
7. Object hierarchy and properties remain usable.
8. A simple material change works.
9. The scene can be saved, closed, and reopened reliably.
10. Actual screenshots or a short video can be captured from the running executable.

Visual direction:
- Familiar Blender workflows; no unnecessary wholesale UI redesign.
- Restrained Hamun branding: typography, icons, accent colors, startup branding, and a few carefully chosen UI details.
- Proposed axis colors: X turquoise, Y orange, Z violet. Verify contrast/accessibility in the real viewport.
- Avoid superficial reskinning that hides broken or untested functionality.

## 6. Recommended development sequence

### Phase 0 — Inspect repository and patch
1. Confirm current branch and repository tree.
2. Fetch the patch and README from the branch.
3. Inspect patch hunks and target source files; the patch context may be stale or malformed.
4. Obtain a full Blender source checkout at commit `2622ed297d669e71dbfdc3478d844ed6004d9285`.
5. Run:
   `git apply --check integrations/blender/patches/0001-hamun-transform-gizmo-colors.patch`
6. Fix the patch against the exact pinned source if it does not apply.
7. Record the actual result and commit any corrected patch/documentation to the feature branch.

Acceptance criterion: patch applies cleanly to the pinned source, or the exact blocker is documented.

### Phase 1 — Establish baseline build
1. Follow Blender's official Windows build instructions for the pinned source revision.
2. Check prerequisites, toolchain, disk space, and build dependencies.
3. Build without the Hamun patch first.
4. Capture exact commands, logs, environment details, and executable path.
5. Launch the baseline editor.

Acceptance criterion: baseline editor launches, or a reproducible blocker is documented.

### Phase 2 — Apply patch and validate visual behavior
1. Apply the validated patch.
2. Build again.
3. Launch the patched executable.
4. Test Move, Rotate, and Scale tools as relevant to the patch implementation.
5. Verify axis colors, selection/hover feedback, contrast, and high-DPI behavior.
6. Capture screenshots only from the real executable.

Acceptance criterion: changes are visible in a running patched build and the result is documented.

### Phase 3 — Build the actual showcase scene
1. Create a small studio scene after the editor build works.
2. Add a floor, a few primitive objects, one imported test asset, camera, and basic lights.
3. Verify selection, transforms, viewport navigation, material editing, save, close, and reopen.
4. Store the actual scene in the repository only when it exists and can be opened.

Acceptance criterion: a fresh build can repeat the showcase demonstration.

### Phase 4 — Reproducible packaging
1. Write `docs/BUILD_WINDOWS.md` with verified prerequisites and commands.
2. Add a Windows build script only after testing its commands in the target environment.
3. Do not commit huge generated build trees or third-party outputs unless repository policy explicitly requires it.
4. Document known limitations and build failures.
5. Capture a short demo from the real build.

Acceptance criterion: another developer can follow the documented instructions and reproduce the working showcase.

## 7. Suggested repository organization

```text
docs/
  ENGINE_SHOWCASE_HANDOFF.md
  BUILD_WINDOWS.md
integrations/
  blender/
    patches/
    README.md
showcase/
  README.md
  Hamun_Showcase.blend   # only after the actual scene exists
scripts/
  build_windows.bat      # only after tested on the target Windows setup
```

Do not create fake placeholder binaries or scripts that claim success without testing.

## 8. Immediate next action for the new chat

Start by inspecting the current patch and README on the feature branch. Fetch the actual files through the GitHub connector, review the patch text, and compare it with the exact Blender source revision. Then determine whether the patch applies cleanly and update it if needed.

Do not start by designing gameplay maps or generating promotional concept images. Do not jump straight to claiming a build exists. The next milestone is a valid patch against a pinned Blender source baseline.

## 9. Useful links

- Repository: https://github.com/lyingtiger88/BDFR_Hamun_Engine
- Feature branch patch: https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/integrations/blender/patches/0001-hamun-transform-gizmo-colors.patch
- Patch README: https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/integrations/blender/patches/README.md
- Correct engine showcase handoff: https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/docs/ENGINE_SHOWCASE_HANDOFF.md
- Earlier map-design handoff (not current scope): https://github.com/lyingtiger88/BDFR_Hamun_Engine/blob/feature/blender-source-integration/docs/MAP_DESIGN_HANDOFF.md
- Blender source repository: https://projects.blender.org/blender/blender

## 10. Operating rule

Keep the handoff truthful and practical. Every progress report should clearly separate:
- **Committed:** actual GitHub changes and commit SHA.
- **Tested:** exact environment/command and observed result.
- **Not tested / blocked:** anything not run or independently verified.
- **Next task:** one concrete action with a measurable acceptance criterion.

The objective is a real Hamun editor showcase that builds and runs, not merely a set of design documents.
