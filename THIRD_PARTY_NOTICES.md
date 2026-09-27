# Third-party notices

## Grit Game Engine

BDFR Hamun Engine is based in part on architectural ideas from the MIT-licensed
Grit Game Engine, especially its open-world resource-demand and background
loading model.

Selected upstream files are retained verbatim under `ThirdParty/GritReference`
for migration/reference and are excluded from the default Hamun build.

Upstream: `grit-engine/grit-engine`
Copyright (c) 2016 The Grit Game Engine authors
License: MIT

Hamun replaces the legacy renderer coupling with a new C++20 RHI designed for
DirectX 12, Vulkan and OpenGL ES, and replaces the old nearest-demand-only
streaming path with a velocity-aware scheduler suitable for vehicles and aircraft.
