# Hamun Test v0.4

Hamun Test v0.4 is the first build where the scene's actual indexed draw
submission is owned by HamunRenderer rather than the Sandbox application.

## Renderer architecture changes

- RenderGraph now belongs to HamunRenderer.
- HamunGraph is again reserved exclusively for visual scripting.
- ComputePass executes before MainRenderPass.
- MainRenderPass owns the graphics binding/draw loop.
- FrameResources keeps two copies of per-frame constant buffers.

## Rendering changes

- glTF metallicFactor and roughnessFactor are imported.
- Material/MaterialInstance parameters feed the scene constants.
- direct lighting uses a GGX NDF, Schlick-GGX geometry term and Schlick Fresnel.
- a no-op compute dispatch validates the DX12/DX11 compute path every frame.
- GPU capability reporting remains enabled.

## Temporal / FSR preparation

- previous view-projection history state
- render/display resolution tracking
- Halton jitter sequence
- history-reset support

The AMD FSR SDK is not bundled in this package yet.

## Hair / TressFX preparation

HamunHair now evaluates the active RHI backend and chooses between:

- Strand path: intended for TressFX on supported DX12/Vulkan + compute builds
- Cards path: compatibility fallback

The external TressFX SDK is not bundled yet.

## Test procedure

Run:

```text
Run_DX12.bat
Run_DX11.bat
```

Verify:

1. The test scene renders in both modes.
2. Camera controls remain functional.
3. No flicker, missing geometry or severe PBR artifacts appear.
4. The console reports the GPU capability block.
5. The console prints `Compute smoke dispatch: enabled` on compute-capable paths.
6. The console prints the temporal foundation jitter line.
7. The console prints the selected hair runtime path.

Controls:

- WASD: move
- Q / E: down / up
- hold right mouse button + mouse: look
- Shift: sprint
- Esc: exit
