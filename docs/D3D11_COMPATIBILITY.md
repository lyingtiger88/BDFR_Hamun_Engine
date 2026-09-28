# Direct3D 11 compatibility backend

Direct3D 11 is implemented as a compatibility backend behind HamunRHI. It is
not the architectural baseline of HamunRenderer.

## Supported RHI path

The current DX11 backend implements the same bootstrap rendering flow used by
DX12:

- vertex buffers
- index buffers
- dynamic constant buffers
- RGBA8 2D textures
- shader-resource views
- sampler states
- vertex and pixel shaders
- input layouts
- depth texture and DSV
- rasterizer/depth state
- indexed rendering
- swap-chain presentation

The Sandbox can select DX11 with:

```text
HamunSandbox.exe --d3d11
```

For CI:

```text
HamunSandbox.exe --smoke-test --d3d11
```

## Feature levels

Device creation attempts these levels in order:

```text
D3D_FEATURE_LEVEL_11_1
D3D_FEATURE_LEVEL_11_0
D3D_FEATURE_LEVEL_10_1
D3D_FEATURE_LEVEL_10_0
```

If a runtime rejects the 11_1 entry, Hamun retries without it. If the hardware
path fails entirely, the backend can fall back to WARP for development/testing.

Shader targets are selected from the negotiated feature level:

- 11_1 / 11_0 -> Shader Model 5.0
- 10_1 -> Shader Model 4.1
- 10_0 -> Shader Model 4.0

## Capability tier

DX11 deliberately reports a reduced capability set compared with DX12/Vulkan.

Not part of the DX11 compatibility target:

- bindless resource model
- mesh shaders
- ray tracing
- explicit asynchronous compute queues
- explicit resource barriers
- DX12-style descriptor heaps

Higher-level renderer code should query HamunRHI capabilities rather than
assuming every backend exposes the modern feature tier.
