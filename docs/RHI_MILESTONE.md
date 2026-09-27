# Hamun RHI milestone

This milestone removes the DirectX-specific bootstrap triangle from the public
Sandbox code and renders it through the backend-neutral Hamun RHI.

## Public objects now available

- `IBuffer`
- `IShader`
- `IPipeline`
- `ICommandList`
- `ISwapChain`
- `IFence`
- `IBackend`

## Current public flow

```text
IBackend
  |
  +-- CreateBuffer
  +-- CreateShader
  +-- CreateGraphicsPipeline
  |
  +-- BeginFrame -> ICommandList
                      |
                      +-- BeginRenderPass
                      +-- SetPipeline
                      +-- SetVertexBuffer
                      +-- Draw
                      +-- EndRenderPass
  |
  +-- SubmitFrame
```

The Sandbox triangle now uses only these interfaces. The Direct3D 12 backend
translates them into native DX12 resources, PSOs, command-list calls, barriers,
queue submission, swap-chain presentation and fence synchronization.

## DX12 implementation status

Implemented behind the RHI:

- upload-backed RHI buffers
- HLSL shader compilation
- root signatures
- input layouts
- graphics PSOs
- command recording
- render-pass clear
- vertex binding
- draw calls
- PRESENT / RENDER_TARGET transitions
- queue execution
- presentation
- per-frame fence synchronization
- swap-chain and fence public views

## Next

The next graphics milestones are:

1. replace the temporary `D3DCompile` path with DXC
2. add index buffers and constant/uniform buffers
3. add textures, samplers and descriptor binding
4. add explicit resource-state tracking
5. implement the same public RHI contract in Vulkan
6. move rendering orchestration into `HamunRenderer`
