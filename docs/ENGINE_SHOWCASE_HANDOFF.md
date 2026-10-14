# Hamun Engine — Engine Showcase Development Handoff

**Purpose:** Continue development of the Hamun Engine editor/engine showcase itself.  
**Branch:** `feature/blender-source-integration`  
**Status:** Planning handoff. This document does not claim that a runnable build or showcase scene already exists.

## Correct scope

This work is about presenting and validating the engine and editor, not designing gameplay maps. The intended foundation is Blender source integration with a Hamun-branded editor experience and a small, repeatable showcase scene.

## Current known repository state

- The repository contains an initial Blender transform-gizmo color patch at `integrations/blender/patches/0001-hamun-transform-gizmo-colors.patch`.
- The patch README documents the integration direction.
- The patch has not yet been verified against a complete Blender source checkout or compiled in this handoff.
- A previous document named `docs/MAP_DESIGN_HANDOFF.md` describes environment-map concepts. Those concepts are not the engine showcase and should not be treated as engine implementation.
- No verified Hamun executable or successful local build is claimed here.

## Showcase target

Deliver one small, polished editor demonstration that proves the integration is real and usable:

1. Launch the built editor from a reproducible build.
2. Open a dedicated `Hamun_Showcase.blend` scene or the agreed native project format.
3. Display a compact studio environment: floor, a few primitive meshes, one imported test asset, a camera, and basic lighting.
4. Select objects in the viewport and use Move, Rotate, and Scale tools.
5. Show Hamun's custom transform-gizmo colors and clear selection/hover states.
6. Demonstrate viewport navigation, object hierarchy, properties, save/reopen, and a simple material change.
7. Capture a short screen recording and screenshots only after the build is verified.

## Visual direction

- Preserve Blender's mature editing workflow where useful; do not redesign every panel before the basic pipeline works.
- Use Hamun identity through restrained typography, iconography, accent colors, startup branding, and a few clearly owned interface details.
- Transform gizmo axes: X turquoise, Y orange, Z violet, as proposed by the existing patch. Confirm accessibility and contrast in the actual viewport.
- Keep layout familiar enough that Blender users can navigate it without relearning the editor.
- Avoid a superficial reskin that hides broken or untested functionality.

## Implementation sequence

### Phase 0 — Establish a reproducible source baseline
- Confirm repository branch and working tree.
- Clone or use a full Blender source checkout at the commit recorded in the patch README.
- Check the patch with `git apply --check`; fix patch context if needed.
- Record exact Blender commit, patch result, build command, compiler/toolchain, and any failures.

**Acceptance:** Patch applies cleanly to the pinned source baseline, or a concrete patch failure is documented.

### Phase 1 — Build an unmodified baseline
- Follow the official Blender build instructions for the target OS.
- Build once without the Hamun patch to separate environment failures from integration failures.
- Save the full logs and resulting executable path.

**Acceptance:** The unmodified editor launches, or the build blocker is reproducible and documented.

### Phase 2 — Apply Hamun identity and rebuild
- Apply the validated patch.
- Build and launch the patched editor.
- Verify the three gizmo axes in the viewport, including Move, Rotate, and Scale where the relevant code path supports the change.
- Check selection, hover, active-tool, dark/light UI contrast, and high-DPI behavior.

**Acceptance:** Visual changes are observable in a real running build, with screenshots and build logs.

### Phase 3 — Create the engine showcase scene
- Create a small scene with clear scale references and a limited number of objects.
- Add one simple material test and one imported asset.
- Verify select, transform, save, close, reopen, and viewport navigation.
- Keep the scene intentionally small so startup and interaction remain responsive.

**Acceptance:** A fresh build can open the showcase scene and repeat the demonstration reliably.

### Phase 4 — Package the demo
- Add a documented build script for the chosen Windows environment only after the commands have been tested.
- Keep third-party source/build outputs out of Git unless repository policy explicitly requires them.
- Document prerequisites, disk-space expectations, build duration as measured, known limitations, and how to reproduce the build.
- Produce screenshots/video from the actual executable, not concept art.

**Acceptance:** Another developer can follow the documented steps and obtain the same working showcase.

## Suggested repository organization

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
  Hamun_Showcase.blend   # add only when the actual scene exists
scripts/
  build_windows.bat      # add only after testing on the target environment
```

Do not add placeholder binary assets or scripts that claim success without being tested.

## Immediate next actions

1. Inspect the current patch and README against the pinned Blender revision.
2. Fix and validate the patch with `git apply --check`.
3. Record exact Windows build prerequisites and commands in `docs/BUILD_WINDOWS.md`.
4. Build the unmodified baseline, then the patched version.
5. Create the showcase scene only after the patched editor launches successfully.

## Handoff reporting rules

For each update, report these separately:
- **Committed:** files and commit SHA actually present in GitHub.
- **Tested:** exact command/environment and observed result.
- **Not tested / blocked:** anything not run locally or not independently verified.
- **Next task:** one concrete action with an observable acceptance criterion.

A design document, patch file, source checkout, successful compilation, and runnable showcase are different milestones. Do not report them as interchangeable.
