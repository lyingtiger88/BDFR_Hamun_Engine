# AMD TressFX Integration Foundation

Hamun treats hair/fur as a first-class engine subsystem rather than embedding
TressFX calls directly in gameplay or scene code.

The new `HamunHair` module provides a backend-neutral runtime plan and keeps
the external SDK optional through:

```text
HAMUN_WITH_TRESSFX_SDK=OFF
```

Current behavior:

- DX12/Vulkan are recognized as the intended native TressFX API paths.
- Compute, async-compute and bindless capabilities are read from HamunRHI.
- Strand simulation/rendering is enabled only when the external TressFX SDK is
  actually linked.
- DX11, GLES/mobile-class targets and builds without TressFX use the card-hair
  fallback path.
- The module compiles on every current Hamun target without requiring TressFX
  source code to be present.

The official public AMD TressFX repository currently exposes TressFX 4.1.0 as
its latest release and provides DirectX 12 and Vulkan implementations.

Next integration work:

1. Vendor or fetch the official TressFX source under ThirdParty.
2. Implement HamunRHI resource adapters for TressFX buffers/textures.
3. Map TressFX simulation dispatches into HamunRenderer ComputePass.
4. Add strand rendering passes to RenderGraph.
5. Add skinning, wind and collision inputs.
6. Add LOD switching and card fallback.
7. Add asset import/export path for TressFX hair data.
