# Hamun transform gizmo source patch

This directory contains the first source-level visual patch for the Blender editor integration.

## Target source

- Upstream: `blender/blender`
- Base commit: `2622ed297d669e71dbfdc3478d844ed6004d9285`
- File changed: `source/blender/editors/transform/transform_gizmo_3d.cc`

## What changes

The X, Y, and Z axes of Blender's native Move, Rotate, and Scale gizmos receive a first-pass Hamun palette:

- X: cyan `(0.10, 0.82, 0.95)`
- Y: orange `(1.00, 0.55, 0.12)`
- Z: violet `(0.65, 0.42, 1.00)`

The patch intentionally preserves native gizmo operators, geometry, hit-testing, dragging, snapping, axis mapping, alpha fading, and the view-aligned center color. This is only the first visual pass; it does not yet create a branded toolbar, workspace, splash screen, or standalone executable.

## Apply and validate

From a Blender source checkout at the exact base commit, copy the patch into the checkout and run:

```sh
git checkout --detach 2622ed297d669e71dbfdc3478d844ed6004d9285
git apply --check /path/to/0001-hamun-transform-gizmo-colors.patch
git apply /path/to/0001-hamun-transform-gizmo-colors.patch
git diff --check
git diff -- source/blender/editors/transform/transform_gizmo_3d.cc
```

Then build Blender using the official build instructions for the chosen operating system and compiler. Launch the resulting executable and manually verify Move, Rotate, and Scale in the 3D View, including axis selection, dragging, snapping, and highlighted states.

## Status and limits

The patch is committed to Hamun's `feature/blender-source-integration` branch, but it has **not yet been applied to a full local Blender checkout, compiled, or runtime-tested**. Therefore it is not yet a testable binary. Do not report a successful build until the build log and manual test are available.

Blender source is GPL-2.0-or-later. Preserve upstream copyright/license notices and review source-distribution obligations before distributing a modified binary.
