# Hamun Test v0.3

Test v0.3 is the first renderer-architecture regression build after v0.2.

## New foundations

- RenderGraph pass registration, compile and execute lifecycle
- MainRenderPass bootstrap owned by HamunRenderer
- runtime Material and MaterialInstance types
- base-color, metallic and roughness parameter foundation
- backend-neutral GPU capability report
- DX11 adapter/memory detection
- DX12 adapter/memory detection
- DX12 resource-binding tier detection
- DX12 mesh-shader detection
- DX12 ray-tracing detection

The visual scene intentionally remains compatible with v0.2. This makes v0.3 a
useful regression test: architecture changes underneath the scene while DX11
and DX12 should continue to render the same content.

## Run

```text
Run_DX12.bat
Run_DX11.bat
```

The console output should include a `Hamun Hardware Report` showing the active
backend, GPU name, memory information and detected capabilities.

## Controls

- WASD: move
- Q / E: down / up
- hold right mouse button + mouse: look
- Shift: sprint
- Esc: exit
