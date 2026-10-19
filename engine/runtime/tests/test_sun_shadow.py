"""DLL acceptance smoke test: directional cast shadow must move with Sun rotation.

Runs against the CPU fallback (the GPU shadow pass is separately pending).
"""
import ctypes
import os
from pathlib import Path

dll_path = Path(os.environ.get("HAMUN_RENDERER_DLL", "build/viewport/Release/HamunRenderer.dll"))
dll = ctypes.WinDLL(str(dll_path.resolve()))
fn = dll.hamun_renderer_mesh_lit_normals
F = ctypes.c_float
U = ctypes.c_uint32
B = ctypes.c_uint8
fn.argtypes = [
    U,U,ctypes.POINTER(B),ctypes.c_uint64,
    ctypes.POINTER(F),U,ctypes.POINTER(U),U,
    ctypes.POINTER(F),ctypes.POINTER(F),U,ctypes.POINTER(F)]
fn.restype = ctypes.c_int

# Floor z=0 and a small raised square z=0.45, viewed orthographically from above.
# The block casts distinct left/right displaced shadows for opposite Sun directions.
verts = [
    (-0.9,-0.9,0), (0.9,-0.9,0), (0.9,0.9,0), (-0.9,0.9,0),
    (-0.13,-0.13,0.45), (0.13,-0.13,0.45),
    (0.13,0.13,0.45), (-0.13,0.13,0.45),
]
triangles=[(0,1,2),(0,2,3),(4,5,6),(4,6,7)]
positions=(F*24)(*(c for v in verts for c in v))
indices=(U*12)(*(i for tri in triangles for i in tri))
normals=(F*12)(*([0,0,1]*4))
# Column-major orthographic MVP; near objects have greater software depth.
matrix=(F*16)(1,0,0,0, 0,1,0,0, 0,0,-1,0, 0,0,0,1)
width=256
height=256

def render(dx):
    pixels=(B*(width*height*4))()
    # Direction vector points from light towards the scene.
    light=(F*12)(0,0,0,0, dx,0,-0.7, 1,1,1, 5,0)
    success=fn(width,height,pixels,len(pixels),positions,8,indices,12,matrix,light,1,normals)
    if not success:
        err=dll.hamun_renderer_last_error
        err.restype=ctypes.c_char_p
        raise RuntimeError(err().decode(errors="replace"))
    return bytes(pixels)

left=render(-0.7)
right=render(0.7)
# Symmetric light directions should yield identical diffuse on the floor,
# so changed floor pixels specifically indicate directional occlusion.
def sample(img,x,y):
    off=4*(y*width+x)
    return img[off:off+3]
# Direction -X casts shadow near x=-0.45 (pixel 70); +X near 0.45 (pixel 186).
# These samples are outside the caster's on-screen top face.
negative_x_in_left=sample(left,70,128)
negative_x_in_right=sample(right,70,128)
positive_x_in_left=sample(left,186,128)
positive_x_in_right=sample(right,186,128)
assert left != right, "Sun rotation produced identical images"
assert sum(negative_x_in_left)<sum(negative_x_in_right), (
    f"Expected darker left receiver for -X Sun: {negative_x_in_left} vs {negative_x_in_right}")
assert sum(positive_x_in_right)<sum(positive_x_in_left), (
    f"Expected darker right receiver for +X Sun: {positive_x_in_right} vs {positive_x_in_left}")
print("PASS: directional shadow moves across the floor as the Sun rotates")
