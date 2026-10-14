# Hamun Authoring Tools for Blender

**Milestone:** first installable Blender-side workflow prototype (0.1.0).

This add-on adds a **Hamun** menu to the 3D View editor header with:
- **Prepare Hamun Scene**: creates/links a `Hamun Assets` collection and adds versioned scene metadata (`hamun_project`, `hamun_schema_version`, `hamun_authoring_profile`).
- **Move / Rotate / Scale**: exposes Blender's native transform operators through the Hamun menu.

It deliberately calls Blender's native transform operators. It does **not yet restyle the on-screen Move/Rotate/Scale gizmos**, alter Blender's C/C++ source, connect the Hamun runtime, or claim a successful Blender runtime test.

## Install

1. Download this repository branch as a ZIP, or download `__init__.py` from this folder.
2. In Blender, open **Edit → Preferences → Add-ons → Install from Disk** (wording varies by Blender version) and select the Python file or ZIP containing this add-on.
3. Enable **Hamun Authoring Tools**.
4. In the 3D View header, open **Hamun**.
5. Select an object and test Move, Rotate, and Scale. Then choose **Prepare Hamun Scene** and inspect the Outliner and Scene Custom Properties.

## Compatibility and validation

- Declared minimum Blender version: 4.0.
- Python syntax is checked in CI-independent validation; a real Blender UI test still requires launching Blender.
- The add-on is GPL-2.0-or-later, matching the intended Blender integration boundary. Preserve this notice if redistributing it.

## Next source-level milestone

Pin a Blender source checkout and implement a narrow C/C++ visual patch to the transform gizmos in `source/blender/editors/transform/transform_gizmo_3d.cc` and the relevant gizmo type implementations. Keep the upstream source and its license notices intact.
