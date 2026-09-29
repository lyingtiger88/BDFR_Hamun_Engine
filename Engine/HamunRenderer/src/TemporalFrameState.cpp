#include <Hamun/Renderer/TemporalFrameState.hpp>

#include <algorithm>

namespace Hamun::Renderer {

void TemporalFrameState::Configure(
    std::uint32_t renderWidth,
    std::uint32_t renderHeight,
    std::uint32_t displayWidth,
    std::uint32_t displayHeight) noexcept
{
    data_.renderWidth =
        std::max(
            renderWidth,
            1u);

    data_.renderHeight =
        std::max(
            renderHeight,
            1u);

    data_.displayWidth =
        std::max(
            displayWidth,
            1u);

    data_.displayHeight =
        std::max(
            displayHeight,
            1u);

    ResetHistory();
}

const TemporalFrameData& TemporalFrameState::BeginFrame(
    const Mat4& currentViewProjection) noexcept
{
    data_.frameNumber =
        nextFrameNumber_;

    data_.historyValid =
        historyValid_;

    data_.currentViewProjection =
        currentViewProjection;

    data_.previousViewProjection =
        historyValid_
            ? previousViewProjection_
            : currentViewProjection;

    data_.previousJitter =
        historyValid_
            ? previousJitter_
            : TemporalJitter{};

    const std::uint64_t sampleIndex =
        nextFrameNumber_ + 1;

    const float jitterX =
        Halton(
            sampleIndex,
            2u) -
        0.5f;

    const float jitterY =
        Halton(
            sampleIndex,
            3u) -
        0.5f;

    data_.currentJitter.xPixels =
        jitterX;

    data_.currentJitter.yPixels =
        jitterY;

    data_.currentJitter.xNdc =
        (2.0f * jitterX) /
        static_cast<float>(
            data_.renderWidth);

    data_.currentJitter.yNdc =
        (-2.0f * jitterY) /
        static_cast<float>(
            data_.renderHeight);

    previousViewProjection_ =
        currentViewProjection;

    previousJitter_ =
        data_.currentJitter;

    historyValid_ = true;
    ++nextFrameNumber_;

    return data_;
}

void TemporalFrameState::ResetHistory() noexcept
{
    nextFrameNumber_ = 0;
    historyValid_ = false;

    previousViewProjection_ = {};
    previousJitter_ = {};

    data_.frameNumber = 0;
    data_.historyValid = false;
    data_.currentJitter = {};
    data_.previousJitter = {};
}

float TemporalFrameState::Halton(
    std::uint64_t index,
    std::uint32_t base) noexcept
{
    if (base < 2u)
        return 0.0f;

    float result = 0.0f;
    float fraction = 1.0f;

    while (index > 0) {
        fraction /=
            static_cast<float>(
                base);

        result +=
            fraction *
            static_cast<float>(
                index % base);

        index /=
            base;
    }

    return result;
}

} // namespace Hamun::Renderer
