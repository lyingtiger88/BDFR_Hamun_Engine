#!/usr/bin/env python3
"""Generate an editable glTF 2.0 atmospheric courtyard for HamunEditor.

Only Python standard library is needed. No external textures or copyrighted assets.
This is reference geometry, NOT an implementation of fog, lighting or rain.
"""
import argparse
import base64
import json
import math
import struct
from pathlib import Path

MATERIALS = [
    ("WetCobblestone", (0.19, 0.24, 0.29, 1), .12, .22),
    ("OldStone", (0.42, 0.43, 0.40, 1), .05, .82),
    ("Bronze", (0.35, 0.23, 0.12, 1), .70, .30),
    ("LampGlass", (1.00, 0.72, 0.32, 1), .05, .14),
    ("StandingWater", (0.09, 0.20, 0.25, 1), .65, .06),
    ("Foliage", (0.09, 0.24, 0.19, 1), .0, .85),
    ("DarkSky", (0.055, 0.075, 0.12, 1), .0, 1),
]
vertices = bytearray()
indices = bytearray()
meshes = []
nodes = []

def box(name, center, size, material):
    """One cuboid as indexed triangles with flat face normals."""
    x,y,z = center
    sx,sy,sz = (v/2 for v in size)
    p = [
        ((0,0,1), [(-sx,-sy,sz),(sx,-sy,sz),(sx,sy,sz),(-sx,sy,sz)]),
        ((0,0,-1), [(sx,-sy,-sz),(-sx,-sy,-sz),(-sx,sy,-sz),(sx,sy,-sz)]),
        ((0,1,0), [(-sx,sy,sz),(sx,sy,sz),(sx,sy,-sz),(-sx,sy,-sz)]),
        ((0,-1,0), [(-sx,-sy,-sz),(sx,-sy,-sz),(sx,-sy,sz),(-sx,-sy,sz)]),
        ((1,0,0), [(sx,-sy,sz),(sx,-sy,-sz),(sx,sy,-sz),(sx,sy,sz)]),
        ((-1,0,0), [(-sx,-sy,-sz),(-sx,-sy,sz),(-sx,sy,sz),(-sx,sy,-sz)]),
    ]
    first_vertex = len(vertices)//32
    for normal, face in p:
        for px,py,pz in face:
            vertices.extend(struct.pack("<8f", px+x,py+y,pz+z,*normal,0.,0.))
    first_index = len(indices)//4
    for f in range(6):
        start = first_vertex+f*4
        indices.extend(struct.pack("<6I",start,start+1,start+2,start,start+2,start+3))
    meshes.append((name, first_vertex, 24, first_index, 36, material))
    nodes.append({"name":name,"mesh":len(meshes)-1})

# Y is up, -Z is toward the courtyard's far end.
box("Courtyard / wet paving",(0,-.16,0),(22,.3,34),0)
box("Courtyard / central stone path",(0,.01,-2),(4,.08,23),1)
for z in (-13,-9,-5,-1,3,7):
    for x in (-1.8,1.8):
        box("Paving edge", (x,.07,z),(0.18,.18,3.8),2)
for x in (-9,9):
    for z in (-13,-5,3,11):
        box("Stone column", (x,2.0,z),(.9,4,.9),1)
        box("Column base", (x,.25,z),(1.35,.5,1.35),1)
        box("Column capital", (x,4.12,z),(1.45,.45,1.45),1)
    box("Arcade roof beam",(x,4.5,-1),(1.5,.6,29),1)
for z in (-12,-4,4,12):
    box("Courtyard crossbeam",(0,4.55,z),(19,.48,.6),1)
for x,z in ((-4,-9),(4,-9),(-4,4),(4,4)):
    box("Lamp / bronze post",(x,1.35,z),(.14,2.7,.14),2)
    box("Lamp / luminous glass marker",(x,2.9,z),(.56,.78,.56),3)
    box("Lamp / crown",(x,3.34,z),(.82,.18,.82),2)
for x,z,w,d in ((-5,-3,2.3,4), (5,1,2.6,3.2), (-5,9,3,2)):
    box("Puddle / reflective reference",(x,.045,z),(w,.025,d),4)
for x,z in ((-7,-10),(7,-10),(-7,8),(7,8)):
    box("Planter",(x,.5,z),(1.3,1,1.3),2)
    box("Shrub silhouette",(x,1.35,z),(1.65,1.3,1.65),5)
box("Courtyard rear wall",(0,2.4,-17),(22,4.8,.8),1)
box("Gate / left pillar",(-2.6,2.5,-16.5),(.7,5,.8),2)
box("Gate / right pillar",(2.6,2.5,-16.5),(.7,5,.8),2)
box("Gate / lintel",(0,5,-16.5),(6,.7,.8),2)

# Keep a single buffer so glTF can be imported without binary companions.
buffer = vertices+indices
buffer_uri = "data:application/octet-stream;base64," + base64.b64encode(buffer).decode("ascii")
vertex_bytes = len(vertices)
gltf = {
 "asset":{"version":"2.0","generator":"Hamun Weather Showcase Python generator"},
 "scene":0,"scenes":[{"name":"Atmospheric Courtyard","nodes":list(range(len(nodes)))}],
 "nodes":nodes,
 "buffers":[{"uri":buffer_uri,"byteLength":len(buffer)}],
 "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":vertex_bytes,"byteStride":32,"target":34962},
    {"buffer":0,"byteOffset":vertex_bytes,"byteLength":len(indices),"target":34963}
 ],
 "materials":[
   {"name":name,"pbrMetallicRoughness":{
       "baseColorFactor":list(color),"metallicFactor":metal,"roughnessFactor":rough}}
   for name,color,metal,rough in MATERIALS
 ],
 "meshes":[{
    "name":name,
    "primitives":[{
      "attributes":{"POSITION":3*m,"NORMAL":3*m+1},
      "indices":3*m+2,"material":material,"mode":4
    }]
 } for m,(name,start,count,first,icount,material) in enumerate(meshes)],
 "accessors":[]
}
# Accessors reference shared vertex/index views but use byte offsets.
for name,start,count,first,icount,material in meshes:
    gltf["accessors"].extend([
      {"bufferView":0,"byteOffset":start*32,"componentType":5126,
       "count":count,"type":"VEC3",
       "min":[min(struct.unpack_from("<f",vertices,(start+j)*32+a*4)[0] for j in range(count)) for a in range(3)],
       "max":[max(struct.unpack_from("<f",vertices,(start+j)*32+a*4)[0] for j in range(count)) for a in range(3)]},
      {"bufferView":0,"byteOffset":start*32+12,"componentType":5126,
       "count":count,"type":"VEC3"},
      {"bufferView":1,"byteOffset":first*4,"componentType":5125,
       "count":icount,"type":"SCALAR"}
    ])
# Indices in buffer are global; shift to local vertices for each mesh.
for name,start,count,first,icount,material in meshes:
    for i in range(first,first+icount):
        old = struct.unpack_from("<I",indices,i*4)[0]
        struct.pack_into("<I",indices,i*4,old-start)
# Correct embedded URI after rebasing mesh indices.
gltf["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(vertices+indices).decode("ascii")
for m in range(len(meshes)):
    gltf["meshes"][m]["primitives"][0]["attributes"] = {"POSITION":m*3,"NORMAL":m*3+1}
    gltf["meshes"][m]["primitives"][0]["indices"] = m*3+2

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output",type=Path,default=Path("Samples/WeatherShowcase/AtmosphericCourtyard.gltf"))
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(gltf,separators=(",",":")),encoding="utf-8")
    print(f"Generated {args.output}: {len(meshes)} named scene objects, {len(vertices)//32} vertices")

if __name__=="__main__":
    main()
