# RHI 3D scene milestone

Hamun's first DirectX 12 test is now a real indexed 3D scene rather than a
hard-coded 2D triangle.

Implemented through the public RHI:

- vertex buffers
- index buffers
- constant buffers
- runtime buffer updates
- root CBV binding
- indexed draws
- depth buffer / DSV
- depth testing and depth writes
- model / view / projection transform
- animated 3D cube
- backend-neutral command recording

The Sandbox still contains no native DirectX calls. All graphics work goes
through HamunRHI.

## Next rendering milestone

- texture resources
- SRV descriptor allocation
- samplers
- texture upload path
- texture/sampler binding in ICommandList
- first textured mesh
- replace bootstrap D3DCompile with DXC
