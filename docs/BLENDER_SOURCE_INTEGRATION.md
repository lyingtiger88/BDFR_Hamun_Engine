# Hamun Blender Source Integration: Initial Audit
# بررسی اولیهٔ یکپارچه‌سازی سورس Blender با هامون

Date: 2026-10-14
Branch: `feature/blender-source-integration`

## Decision

Use the real Blender source as the editor foundation. Do not continue treating the existing Win32 `Tools/HamunEditor` as the intended replacement for Blender's full editor. Keep it as the current Hamun-native prototype unless a later architecture decision retires it.

از سورس واقعی Blender به‌عنوان پایهٔ ادیتور استفاده می‌کنیم. مسیر فعلی `Tools/HamunEditor` که با کنترل‌های Win32 ساخته شده نباید جایگزین نهایی کل ادیتور Blender فرض شود؛ فعلاً آن را به‌عنوان نمونهٔ اولیهٔ بومی هامون نگه می‌داریم تا تصمیم معماری بعدی.

## Verified source observations

- Official upstream repository: https://github.com/blender/blender
- Upstream branch inspected: `main`
- Upstream commit observed during this audit: `2622ed297d669e71dbfdc3478d844ed6004d9285`
- File inspected: `source/blender/editors/space_view3d/view3d_gizmo_navigate.cc`
- That file defines the View3D navigation gizmo group and includes Blender GPL-2.0-or-later source notices.
- Hamun's current editor entry point is `Tools/HamunEditor/src/main.cpp`; it currently implements a native Win32 UI and a renderer preview. This is not Blender's editor framework.
- No Blender source has been copied into this repository and no Blender build has been run as part of this audit.

## First UI patch scope

1. Create a separately tracked Blender source checkout/fork pinned to a known upstream commit.
2. Establish a repeatable baseline build before changing behavior.
3. Add Hamun-specific workspace defaults and visual theme choices.
4. Identify the actual transform gizmo drawing/interaction code for Move, Rotate, and Scale. The navigation gizmo file above is not itself proof that transform gizmos are implemented there; trace their registrations and implementations before patching.
5. Make the first patch small and reviewable: change styling/visual details while preserving axis semantics, snapping, drag behavior, keyboard shortcuts, and accessibility/contrast.
6. Build and manually test the patched UI before adding the engine bridge.

## Engine integration boundary

Treat the Blender-based authoring application and Hamun runtime as separate components at first. Define an explicit interchange/bridge for scene data, meshes, materials, skeletons, animation clips, and asset identifiers. Prefer a versioned interchange format or a narrow API boundary before attempting to replace Blender's internal renderer or scene model. Import/export interoperability is a milestone, not proof of a shared runtime scene.

## Licensing and release checklist

- Preserve Blender copyright, attribution, and GPL notices.
- Determine how the modified Blender application and any bridge are distributed; obtain appropriate legal review for combined-work questions.
- Publish the corresponding source and license materials required for the chosen GPL distribution model.
- Keep Hamun's existing MIT license statements scoped accurately; do not present Blender-derived code as MIT-only.
- Do not distribute a modified binary until its source/build provenance and license materials are ready.

## Next verifiable milestones

- [x] Pin and document an upstream Blender commit.
- [ ] Record supported OS/compiler/dependency prerequisites and a reproducible baseline build.
- [x] Trace transform gizmo implementation files and identify a minimal visual patch.
- [ ] Implement Hamun workspace/theme customization on the Blender source branch.
- [ ] Build and manually verify Move, Rotate, and Scale.
- [ ] Design and test the first versioned Hamun asset interchange path.

## Current limitations

This is a source audit and implementation plan, not a completed integration. Access to the public source is confirmed, but a compiled binary, visual patch, runtime bridge, and end-to-end test have not been delivered yet.


## Milestone 1 delivered: Blender-side authoring add-on (0.1.0)

Committed under `integrations/blender/` on this branch:

- `hamun_authoring_tools/__init__.py`: adds a Hamun menu to the 3D View header.
- The menu exposes Blender's native Move, Rotate, and Scale operators, preserving their built-in behavior.
- **Prepare Hamun Scene** creates/links a `Hamun Assets` collection and stores `hamun_project`, `hamun_schema_version=1`, and `hamun_authoring_profile` scene properties.
- `tests/test_addon_source.py`: four static tests pass locally for Python parsing, registration entry points, transform operator presence, and versioned metadata.
- `integrations/blender/README.md`: installation and manual validation steps.

This is an installable add-on prototype, not yet the requested C/C++ gizmo reskin. No Blender executable was available in this workflow, so the add-on has **not** been runtime-tested inside Blender. The next milestone remains a source-level visual change to the actual transform gizmo implementations, followed by a real Blender build and manual UI test.
