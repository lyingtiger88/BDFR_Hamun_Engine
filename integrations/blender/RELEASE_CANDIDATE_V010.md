# Hamun Editor v0.1.0 Alpha — pre-release candidate

**Status: internal candidate only. Do not publish this document as proof of release readiness.**

## Included
- Windows x64 portable editor based on modified Blender 5.4 Alpha.
- Hamun branding, splash artwork and Windows launcher.
- Hamun Authoring Tools: project creation, save/open, asset GLB import/link,
  scene export/import in experimental `.hscene` format.
- Portable startup integration and scripted packaging with SHA256 output.

## Known limitations
- The editor is based on Blender; it is **not** yet a standalone Hamun gameplay runtime.
- `.hscene` v1 stores entity metadata/hierarchy/transforms; it does not serialize
  complete geometry, materials, lights or animations for runtime playback.
- Blender version and build provenance are retained for attribution.
- Clean Windows machine validation has not yet been recorded.

## Release gates (must be completed before making a public GitHub Release)
- [ ] Extract the packaged ZIP and launch on another clean Windows x64 machine.
- [ ] Verify Hamun menu, startup integration and create/save/reopen a project.
- [ ] Verify GLB workflow and `.hscene` export/import.
- [ ] Confirm license files and upstream copyright notices.
- [ ] Provide corresponding modified Blender source, build scripts and patches
      sufficient for recipients to rebuild the shipped binary; review the GPL
      requirements for both Blender and bundled dependencies.
- [ ] Verify that patches 0001 and 0002 apply in the documented order to the
      exact pinned Blender base (without applying either patch twice).
- [ ] Confirm the final ZIP's SHA256 and publish the matching checksum.
- [ ] Mark any GitHub Release as **pre-release**, with current limitations.

## Build output
Run `integrations/blender/package_windows.ps1` as documented in
`integrations/blender/PACKAGING_WINDOWS.md`. The output ZIP and hash
are generated **locally**, not uploaded automatically.

## Source provenance
- Blender base commit: `2622ed297d669e71dbfdc3478d844ed6004d9285`.
- Hamun integration source: this repository, branch
  `feature/blender-source-integration`.
- Source patch inventory: `integrations/blender/patches/`.

Never substitute a patch-only archive for corresponding source without
checking its legal and practical sufficiency for that exact binary.
