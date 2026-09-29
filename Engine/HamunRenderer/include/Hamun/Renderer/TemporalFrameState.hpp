#pragma once

#include <Hamun/Renderer/FreeCamera.hpp>

#include <cstdint>

namespace Hamun::Renderer {

struct TemporalJitter {
    float xPixels = 0.0f;
    float yPixels = 0.0f;
    float xNdc = 0.0f;
    float yNdc = 0.0f;
};

struct TemporalFrameData {
    Mat4 currentViewProjection{};
    Mat4 previousViewProjection{};

    TemporalJitter currentJitter{};
    TemporalJitter previousJitter{};

    std::uint64_t frameNumber = 0;

    std::uint32_t renderWidth = 1;
    std::uint32_t renderHeight = 1;
    std::uint32_t displayWidth = 1;
    std::uint32_t displayHeight = 1;

    bool historyValid = false;
};

class TemporalFrameState {
public:
    void Configure(
        std::uint32_t renderWidth,
        std::uint32_t renderHeight,
        std::uint32_t displayWidth,
        std::uint32_t displayHeight) noexcept;

    const TemporalFrameData& BeginFrame(
        const Mat4& currentViewProjection) noexcept;

    const TemporalFrameData& BeginFrame(
        const Mat4& view,
        const Mat4& projection) noexcept;

    void ResetHistory() noexcept;

    [[nodiscard]] const TemporalFrameData& Data() const noexcept
    {
        return data_;
    }

private:
    static float Halton(
        std::uint64_t index,
        std::uint32_t base) noexcept;

    TemporalFrameData data_{};
    Mat4 previousViewProjection_{};
    TemporalJitter previousJitter_{};

    std::uint64_t nextFrameNumber_ = 0;
    bool historyValid_ = false;
};

} // namespace Hamun::Renderer
