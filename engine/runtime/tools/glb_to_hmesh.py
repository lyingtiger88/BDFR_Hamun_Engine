#!/usr/bin/env python3
"""Convert basic embedded-geometry GLB 2.0 to Hamun HMESH v1.

Supported: triangle primitives, FLOAT VEC3 POSITION/NORMAL, UBYTE/USHORT/UINT
scalar indices, embedded GLB BIN. No skins, animation, sparse accessors,
quantization, external buffers, textures, or node hierarchy yet.
"""
import argparse, json, struct
from pathlib import Path

def convert(src: Path, dst: Path):
    data=src.read_bytes()
    if len(data)<20 or data[:4]!=b"glTF": raise ValueError("Input is not GLB")
    magic, version, length=struct.unpack_from("<4sII",data)
    if version!=2 or length!=len(data):raise ValueError("Invalid GLB length/version")
    pos=12; doc=None; binchunk=None
    while pos+8<=len(data):
        size,kind=struct.unpack_from("<I4s",data,pos);pos+=8
        if pos+size>len(data):raise ValueError("Truncated GLB chunk")
        content=data[pos:pos+size];pos+=size
        if kind==b"JSON":doc=json.loads(content)
        elif kind==b"BIN\x00":binchunk=content
    if not isinstance(doc,dict) or binchunk is None:raise ValueError("GLB JSON/BIN missing")
    buffers=doc.get("buffers",[])
    if len(buffers)!=1 or buffers[0].get("uri"):raise ValueError("Only embedded GLB BIN supported")
    if len(binchunk)<buffers[0]["byteLength"]:raise ValueError("Truncated BIN")
    views=doc.get("bufferViews",[]);accessors=doc.get("accessors",[])
    def read_accessor(index, expected_type, expected_component):
        a=accessors[index]
        if a.get("sparse") or a.get("normalized") or a["type"]!=expected_type or a["componentType"]!=expected_component:
            raise ValueError("Unsupported sparse/type/component/normalized accessor")
        v=views[a["bufferView"]]
        if v.get("buffer",0)!=0:raise ValueError("External buffer view")
        components=3 if expected_type=="VEC3" else 1
        fmt={5126:"f",5125:"I",5123:"H",5121:"B"}[expected_component]
        unit=struct.calcsize("<"+fmt)*components
        stride=v.get("byteStride",unit)
        if stride<unit: raise ValueError("Bad stride")
        start=v.get("byteOffset",0)+a.get("byteOffset",0)
        end=v.get("byteOffset",0)+v["byteLength"]
        count=a["count"]
        if count and (start+(count-1)*stride+unit>end or start+(count-1)*stride+unit>len(binchunk)):
            raise ValueError("Accessor outside buffer view")
        return [struct.unpack_from("<"+fmt*components,binchunk,start+i*stride) for i in range(count)]
    vertices=[];indices=[]
    for mesh in doc.get("meshes",[]):
        for primitive in mesh.get("primitives",[]):
            if primitive.get("mode",4)!=4:continue
            attr=primitive.get("attributes",{})
            if "POSITION" not in attr:continue
            positions=read_accessor(attr["POSITION"],"VEC3",5126)
            normals=(read_accessor(attr["NORMAL"],"VEC3",5126) if "NORMAL" in attr else [(0.0,1.0,0.0)]*len(positions))
            if len(positions)!=len(normals):raise ValueError("Position/normal count mismatch")
            if "indices" in primitive:
                acc=accessors[primitive["indices"]]
                if acc["componentType"] not in (5121,5123,5125):raise ValueError("Unsupported index component")
                raw=read_accessor(primitive["indices"],"SCALAR",acc["componentType"])
                local=[value[0] for value in raw]
            else:local=list(range(len(positions)))
            if len(local)%3:raise ValueError("Indices must be triangles")
            if any(i>=len(positions) for i in local):raise ValueError("Index outside vertex range")
            base=len(vertices)
            vertices.extend((*p,*n) for p,n in zip(positions,normals))
            indices.extend(base+i for i in local)
    if not vertices or not indices:raise ValueError("No supported triangle mesh in GLB")
    if len(vertices)>2_000_000 or len(indices)>6_000_000:raise ValueError("Mesh too large for v0.1")
    with dst.open("wb") as f:
        f.write(struct.pack("<4sIII",b"HMSH",1,len(vertices),len(indices)))
        for vertex in vertices:f.write(struct.pack("<6f",*vertex))
        for index in indices:f.write(struct.pack("<I",index))
    print(f"Converted {src} -> {dst}: {len(vertices)} vertices, {len(indices)//3} triangles")
if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("source",type=Path);ap.add_argument("output",type=Path)
    args=ap.parse_args()
    convert(args.source,args.output)
