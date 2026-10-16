# SPDX-License-Identifier: GPL-2.0-or-later
"""Validate a HamunScene v1 JSON file before handing it to a future C++ runtime.

Usage: py -3 integrations/blender/tests/validate_hscene.py path/to/Main.hscene
This is a format validator, NOT a rendering or gameplay runtime.
"""
import json
import math
import sys
from pathlib import Path


def validate(doc):
    errors = []
    if not isinstance(doc, dict):
        return ["Root must be an object"]
    if doc.get("format") != "HamunScene":
        errors.append("format must be HamunScene")
    if type(doc.get("schema_version")) is not int or doc["schema_version"] != 1:
        errors.append("schema_version must equal integer 1")
    if doc.get("coordinate_system") != "Blender_Z_UP":
        errors.append("coordinate_system must be Blender_Z_UP")
    if doc.get("transform_space") != "world":
        errors.append("transform_space must be world")
    entities = doc.get("entities")
    if not isinstance(entities, list):
        return errors + ["entities must be an array"]
    ids = {}
    for index, entity in enumerate(entities):
        pre = f"entities[{index}]"
        if not isinstance(entity, dict):
            errors.append(f"{pre} must be an object")
            continue
        eid = entity.get("id")
        if not isinstance(eid, str) or not eid:
            errors.append(f"{pre}.id must be a nonempty string")
        elif eid in ids:
            errors.append(f"{pre}.id is duplicated: {eid}")
        else:
            ids[eid] = entity
        if not isinstance(entity.get("name"), str) or not entity["name"]:
            errors.append(f"{pre}.name must be a nonempty string")
        if not isinstance(entity.get("type"), str) or not entity["type"]:
            errors.append(f"{pre}.type must be a nonempty string")
        if entity.get("transform_space") != "world":
            errors.append(f"{pre}.transform_space must be world")
        for key, count in (("position", 3), ("rotation_quaternion", 4), ("scale", 3)):
            v = entity.get(key)
            if not isinstance(v, list) or len(v) != count or any(
                isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) for x in v
            ):
                errors.append(f"{pre}.{key} must contain {count} finite numbers")
        parent = entity.get("parent")
        if parent is not None and (not isinstance(parent, str) or not parent):
            errors.append(f"{pre}.parent must be null or a nonempty ID")
    # Cross-reference and cycle checks.
    for eid, entity in ids.items():
        parent = entity.get("parent")
        if isinstance(parent, str) and parent not in ids:
            errors.append(f"Missing parent {parent!r} for {eid!r}")
        seen = {eid}
        while isinstance(parent, str) and parent in ids:
            if parent in seen:
                errors.append(f"Parent cycle detected from {eid!r}")
                break
            seen.add(parent)
            parent = ids[parent].get("parent")
    return errors


def main(argv):
    if len(argv) != 2:
        print("Usage: py -3 validate_hscene.py Main.hscene", file=sys.stderr)
        return 2
    try:
        doc = json.loads(Path(argv[1]).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        print("INVALID:", exc, file=sys.stderr)
        return 1
    errors = validate(doc)
    if errors:
        for error in errors:
            print("INVALID:", error, file=sys.stderr)
        return 1
    print(f"VALID HamunScene v1: {len(doc['entities'])} entities")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
