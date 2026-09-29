# Native RHI Interop

Hamun keeps API-specific objects behind HamunRHI during normal rendering.
External GPU middleware such as AMD FSR and TressFX sometimes requires native
API objects. For those integrations, HamunRHI exposes a deliberately small
interop surface.

Available handles:

- backend native device
- backend native command queue / immediate context
- command-list native handle
- swap-chain native handle where supported
- buffer native resource
- texture native resource

The handles are opaque `void*` values at the public RHI boundary. Integration
modules must check `BackendType` before casting them to a native API type.

This interop path is for engine integrations only. Gameplay code and normal
renderer passes should continue using backend-neutral RHI interfaces.

Primary consumers:

- Hamun FSR bridge: D3D12 device, queue, command list, textures
- HamunHair / TressFX bridge: D3D12/Vulkan device resources and dispatch path
- future vendor-specific diagnostic/profiling integrations
