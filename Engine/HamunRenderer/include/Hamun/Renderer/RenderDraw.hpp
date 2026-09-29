#pragma once

#include <Hamun/RHI/RHI.hpp>

#include <cstdint>

namespace Hamun::Renderer {

struct IndexedDraw {
    RHI::IBuffer* vertexBuffer = nullptr;
    RHI::IBuffer* indexBuffer = nullptr;
    RHI::IBuffer* constantBuffer = nullptr;
    RHI::ITexture* texture = nullptr;

    std::uint32_t vertexStride = 0;
    std::uint32_t indexCount = 0;
    RHI::IndexType indexType = RHI::IndexType::UInt32;
};

} // namespace Hamun::Renderer
