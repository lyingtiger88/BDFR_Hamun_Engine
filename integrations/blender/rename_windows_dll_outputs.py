# SPDX-License-Identifier: GPL-2.0-or-later
"""Guarded Windows Hamun DLL output naming. Dry-run unless --apply.

Keeps the CMake target identities intact and changes only the DLL filenames.
No DLLs are renamed in File Explorer. Must be rebuilt and tested afterwards.
"""
from pathlib import Path
import argparse

EDITS = {
    "source/blender/cpucheck/CMakeLists.txt": [
        ('  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${_LIB_SUB_FOLDER}"', '  OUTPUT_NAME "hamun_cpu_check"\n  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${_LIB_SUB_FOLDER}"')
    ],
    "source/blender/blendthumb/CMakeLists.txt": [
        ('set_target_properties(BlendThumb PROPERTIES LINK_FLAGS "/OPT:ref")', 'set_target_properties(BlendThumb PROPERTIES OUTPUT_NAME "HamunThumb")\n  set_target_properties(BlendThumb PROPERTIES LINK_FLAGS "/OPT:ref")')
    ],
    "source/blender/blenlib/intern/winstuff.cc": [
        ('const char *thumbnail_handler = "BlendThumb.dll";', 'const char *thumbnail_handler = "HamunThumb.dll";')
    ],
}

def main():
    p = argparse.ArgumentParser()
    p.add_argument("source", type=Path)
    p.add_argument("--apply", action="store_true")
    args = p.parse_args()
    writes = []
    for rel, changes in EDITS.items():
        path = args.source / rel
        if not path.exists():
            raise SystemExit("Missing " + str(path))
        original = path.read_bytes()
        data = original
        for old, new in changes:
            o, n = old.encode(), new.encode()
            count = data.count(o)
            if count == 0 and n in data:
                print("Already changed:", rel)
                continue
            expected = 2 if "thumbnail_handler" in old else 1
            if count != expected:
                raise SystemExit(f"Unexpected occurrences in {rel}: expected {expected}, found {count}; nothing written")
            data = data.replace(o, n)
            print(f"{rel}: {count} replacement(s)")
        if data != original:
            writes.append((path, original, data))
    for path, original, _ in writes:
        backup = path.with_name(path.name + ".hamun-dllnames.bak")
        if backup.exists() and backup.read_bytes() != original:
            raise SystemExit("Backup conflict: " + str(backup))
    if not args.apply:
        print("DRY RUN; no changes made. Re-run with --apply.")
        return
    for path, original, data in writes:
        backup = path.with_name(path.name + ".hamun-dllnames.bak")
        if not backup.exists():
            backup.write_bytes(original)
        path.write_bytes(data)
    print("APPLIED:", len(writes), "files. Do not rename binaries manually.")
if __name__ == "__main__":
    main()
