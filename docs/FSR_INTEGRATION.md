# AMD FSR Integration Foundation

Hamun keeps temporal upscaling behind renderer/RHI abstractions instead of
hard-wiring one SDK into scene code.

Current engine prerequisites implemented:

- renderer-owned frame submission
- RenderGraph pass ordering
- DX12/DX11 compute pipeline and dispatch foundation
- double-buffered frame resources
- temporal frame history state
- previous view-projection tracking
- Halton jitter generation
- separate render and display dimensions
- PBR material/lighting foundation

Still required before enabling the production FSR path:

- motion-vector render target
- shader-readable depth
- reactive/transparency mask generation where required
- exposure/luminance input strategy
- GPU storage/UAV resource bindings
- resource-state/barrier tracking in RenderGraph
- resolution-change and camera-cut history reset
- official AMD FSR SDK integration and backend bridge

As of September 2026, AMD's public FSR SDK repository identifies SDK 2.3.0
as the current release and includes the FSR 4.1.1 ML upscaler alongside
FSR 3.x and FSR 2.x components. Hamun will keep a generic upscaler interface
so the engine is not permanently coupled to one FSR generation.

The initial production integration should target the primary DX12 path first.
Vulkan support must be evaluated against the exact SDK component/version used
at integration time instead of being assumed from the DX12 implementation.
