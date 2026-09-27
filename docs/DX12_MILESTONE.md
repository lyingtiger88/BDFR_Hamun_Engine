# DirectX 12 bootstrap milestone

This milestone turns the original HamunRHI placeholder into a functioning
Direct3D 12 bootstrap renderer on Windows.

Implemented:

- Win32 native window
- DXGI factory and high-performance adapter selection
- WARP fallback
- Direct3D 12 device
- direct command queue
- double-buffered flip-discard swap chain
- RTV descriptor heap and back-buffer RTVs
- per-frame command allocators
- graphics command list
- GPU fence synchronization
- root signature
- graphics pipeline state object
- bootstrap HLSL vertex/pixel shaders
- upload-heap vertex buffer
- viewport/scissor state
- resource state transitions
- clear + first RGB triangle
- Present
- CI smoke-test mode

## Temporary shader compiler

The bootstrap triangle uses `D3DCompile` from the Windows SDK to avoid
introducing a separate binary dependency in the first DX12 milestone.

The planned production shader pipeline remains:

```text
HLSL
  +-- DXC -> DXIL   -> DirectX 12
  +-- DXC -> SPIR-V -> Vulkan
                   -> translation path -> GLSL ES
```

The next RHI milestone should introduce explicit Hamun objects for buffers,
textures, pipelines, command lists and synchronization so the triangle no
longer depends on backend-internal demo rendering.
