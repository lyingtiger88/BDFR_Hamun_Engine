#pragma once

#include <Hamun/Platform/Window.hpp>

namespace Hamun::Renderer {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    float m[16]{};
};

Mat4 Multiply(
    const Mat4& a,
    const Mat4& b);

class FreeCamera {
public:
    void Update(
        Platform::IWindow& window,
        float deltaSeconds);

    [[nodiscard]] Mat4 ViewMatrix() const;
    [[nodiscard]] Mat4 ProjectionMatrix(
        float aspectRatio) const;

    [[nodiscard]] Mat4 ViewProjection(
        float aspectRatio) const;

    void MoveLocal(
        float forward,
        float right,
        float up) noexcept;

    void Rotate(
        float yawDelta,
        float pitchDelta) noexcept;

    void SetPosition(
        const Vec3& position) noexcept
    {
        position_ = position;
    }

    [[nodiscard]] const Vec3& Position() const noexcept
    {
        return position_;
    }

    float moveSpeed = 2.5f;
    float sprintMultiplier = 4.0f;
    float mouseSensitivity = 0.0025f;
    float verticalFovRadians = 1.0471975512f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

private:
    Vec3 position_{
        0.0f,
        0.0f,
        -3.0f
    };

    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
};

} // namespace Hamun::Renderer
