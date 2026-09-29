# HamunUpscale / AMD FSR Bridge

HamunUpscale is the engine-facing temporal upscaling module.

The current bridge targets AMD FSR SDK 2.3.0 and its current public upscaler API.
The SDK uses a small DLL ABI (`ffxCreateContext`, `ffxDestroyContext`,
`ffxDispatch`, `ffxQuery`, `ffxConfigure`) and AMD recommends loading the
DLL at runtime.

Hamun probes these official runtime files beside the executable:

```text
amd_fidelityfx_loader_dx12.dll
amd_fidelityfx_upscaler_dx12.dll
```

Normal Hamun builds do not require the SDK.

To compile the typed FSR context bridge:

```text
cmake -S . -B build ^
  -DHAMUN_WITH_FSR_SDK=ON ^
  -DHAMUN_FSR_SDK_DIR=C:/SDK/FidelityFX-SDK
```

The SDK root is expected to contain:

```text
Kits/FidelityFX/api/include
Kits/FidelityFX/upscalers/include
Kits/FidelityFX/signedbin
```

Current completed bridge work:

- DX12 native device/queue/command-list/resource interop
- FSR runtime DLL probing
- required ABI symbol resolution
- official FSR upscaler version descriptor support when SDK headers are enabled
- FSR context creation/destruction path
- NativeAA / Quality / Balanced / Performance / Ultra Performance sizing policy
- temporal GPU resource allocation foundation

Still required for live image upscaling:

- render scene color to an offscreen HDR texture
- render true depth and motion vectors
- resource-state transitions / RenderGraph barriers
- map Hamun textures to FfxApiResource descriptors at dispatch
- call ffxDispatch before presentation
- present the upscaled output texture
- package the signed AMD runtime DLLs for an FSR-enabled test build

The current AMD FSR SDK documentation states that backend-specific API support
for this DLL path is currently DirectX 12. Hamun therefore keeps the integration
behind HamunUpscale rather than assuming the same path for Vulkan.
