# SPDX-License-Identifier: GPL-2.0-or-later
"""Audit residual Blender-branded resources in a staged Hamun Windows package.

Preserves all files. Binary translation catalogs cannot be renamed independently
of the compiled gettext domain. Outputs a machine-readable report and summary.
"""
import argparse
import json
from pathlib import Path

RISK = {
    "blender.mo": "high: gettext catalogs; require matching engine domain changes",
    "blender_default.py": "high: active keymap generator used by Hamun",
    "blender_assets.cats.txt": "high: asset catalog format filename",
    "blender_manifest.toml": "high: addon manifest filename and packaging conventions",
}
def classify(relative):
    path = relative.as_posix().lower()
    name = relative.name.lower()
    if name in RISK:
        return "retain_for_compatibility", RISK[name]
    if "/io_scene_gltf2/blender/" in "/" + path + "/":
        return "retain_for_compatibility", "Python import namespace for glTF tools"
    if "license" in relative.parts:
        return "retain_for_compliance", "upstream attribution or third-party legal text"
    if name.endswith((".py", ".pyi", ".yaml")):
        return "needs_reference_audit", "module/import or tooling reference may depend on filename"
    return "needs_reference_audit", "verify references and packaging before renaming"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("package_dir", type=Path)
    args = p.parse_args()
    root = args.package_dir.resolve()
    if not (root / "HamunEditor.exe").is_file():
        raise SystemExit("Expected a staged HamunEditor.exe; refusing to audit wrong directory.")
    hits = []
    for item in root.rglob("*"):
        if not item.is_file() or "blender" not in item.name.lower():
            continue
        relative = item.relative_to(root)
        category, reason = classify(relative)
        hits.append(dict(path=relative.as_posix(), category=category, reason=reason))
    report = {"purpose":"diagnostic inventory; NO file renames", "total":len(hits),
              "files":sorted(hits,key=lambda i:i["path"])}
    output = root / "HAMUN_BRANDING_RESOURCE_AUDIT.json"
    output.write_text(json.dumps(report, ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"HAMUN resource audit: {len(hits)} residual filenames with Blender; report: {output.name}")
    print("Translation catalog names preserved until compiled gettext domain is migrated.")
if __name__ == "__main__":
    main()
