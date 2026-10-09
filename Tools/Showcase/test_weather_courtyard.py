#!/usr/bin/env python3
"""Smoke test of the reproducible atmospheric glTF scene."""
import base64
import json
import subprocess
import sys
import tempfile
from pathlib import Path

generator=Path(__file__).resolve().with_name("generate_weather_courtyard.py")
with tempfile.TemporaryDirectory() as temp:
    output=Path(temp)/"Courtyard.gltf"
    subprocess.run([sys.executable,str(generator),"--output",str(output)],check=True)
    scene=json.loads(output.read_text(encoding="utf-8"))
    assert scene["asset"]["version"]=="2.0"
    assert len(scene["meshes"])>=40
    assert len(scene["nodes"])==len(scene["meshes"])
    uri=scene["buffers"][0]["uri"]
    data=base64.b64decode(uri.split(",",1)[1],validate=True)
    assert len(data)==scene["buffers"][0]["byteLength"]
    for mesh in scene["meshes"]:
        primitive=mesh["primitives"][0]
        for index in (*primitive["attributes"].values(),primitive["indices"]):
            assert 0<=index<len(scene["accessors"])
        positions=scene["accessors"][primitive["attributes"]["POSITION"]]
        assert len(positions["min"])==3 and len(positions["max"])==3
    print("Showcase geometry smoke test passed")
