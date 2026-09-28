# Textured RHI mesh milestone

This milestone adds the first complete sampled-texture path to HamunRHI.

## Public RHI additions

- `ITexture`
- `ISampler`
- `TextureDesc`
- `SamplerDesc`
- `IBackend::CreateTexture`
- `IBackend::CreateSampler`
- `ICommandList::SetTexture`
- `ICommandList::SetSampler`

## DirectX 12 implementation

- default-heap 2D texture resources
- upload-buffer staging
- `GetCopyableFootprints`
- row-pitch-aware texture upload
- `CopyTextureRegion`
- COPY_DEST -> PIXEL_SHADER_RESOURCE transition
- shader-visible CBV/SRV/UAV descriptor heap
- shader-visible sampler descriptor heap
- SRV creation
- sampler creation
- descriptor-table root parameters
- texture/sampler binding through HamunRHI

The texture upload follows the Direct3D 12 staging model: CPU-visible upload
buffers feed GPU-local texture resources and use CopyTextureRegion before the
resource is transitioned for shader sampling.

## DXC

Hamun now attempts to compile HLSL through the DirectX Shader Compiler (DXC)
using Shader Model 6.0. The DXC DLL is loaded dynamically, so the engine does
not gain a hard link-time dependency.

During the early bootstrap stage, a Shader Model 5.1 D3DCompile fallback is
retained for development machines where DXC is unavailable. The fallback is
temporary and should be removed once Hamun's shader cooker/tool distribution is
implemented.

## Visual test

The Windows Sandbox now renders a rotating indexed cube with UV coordinates and
a procedurally generated checkerboard texture. The Sandbox itself contains no
DirectX API calls; texture creation, sampling, descriptor binding and drawing
all go through HamunRHI.

## Next

- move shader compilation into a dedicated Hamun shader/compiler service
- texture asset decoding and file loading
- mipmaps
- immutable/default-heap geometry uploads
- descriptor allocator/free-list instead of monotonic bootstrap allocation
- first glTF mesh importer
- basic camera/input system
