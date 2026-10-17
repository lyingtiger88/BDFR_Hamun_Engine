#pragma once
#include <array>
#include <cstdint>
#include <vector>

// v0.3: CPU-side mesh data. GPU upload is owned by the Vulkan runtime.
struct HamunVertex { float x,y,z; float r,g,b; };
struct HamunMesh { std::vector<HamunVertex> vertices; std::vector<uint32_t> indices; };
inline HamunMesh hamunMakeCube(){
    const std::array<std::array<float,3>,8> p{{
      {{-1,-1,-1}},{{1,-1,-1}},{{1,1,-1}},{{-1,1,-1}},
      {{-1,-1,1}},{{1,-1,1}},{{1,1,1}},{{-1,1,1}}
    }};
    const std::array<uint32_t,36> idx{{
      4,5,6,4,6,7, 1,0,3,1,3,2, 0,4,7,0,7,3,
      5,1,2,5,2,6, 3,7,6,3,6,2, 0,1,5,0,5,4
    }};
    const std::array<std::array<float,3>,6> c{{
      {{0.18f,0.8f,0.9f}},{{0.95f,0.3f,0.35f}},{{0.4f,0.95f,0.45f}},
      {{1,0.72f,0.25f}},{{0.55f,0.45f,1}},{{0.85f,0.85f,0.9f}}
    }};
    HamunMesh mesh;
    for(uint32_t i=0;i<36;i++){
      const auto& v=p[idx[i]]; const auto& col=c[i/6];
      mesh.vertices.push_back({v[0],v[1],v[2],col[0],col[1],col[2]});
      mesh.indices.push_back(i);
    }
    return mesh;
}
