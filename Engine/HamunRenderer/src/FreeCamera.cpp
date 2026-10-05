#include <Hamun/Renderer/FreeCamera.hpp>

#include <algorithm>
#include <cmath>

namespace Hamun::Renderer {
namespace {

float Dot(
    const Vec3& a,
    const Vec3& b)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

Vec3 Cross(
    const Vec3& a,
    const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 Normalize(
    const Vec3& value)
{
    const float lengthSquared =
        Dot(value, value);

    if (lengthSquared <= 0.000001f)
        return {};

    const float invLength =
        1.0f /
        std::sqrt(lengthSquared);

    return {
        value.x * invLength,
        value.y * invLength,
        value.z * invLength
    };
}

Vec3 ForwardVector(
    float yaw,
    float pitch)
{
    const float cosPitch =
        std::cos(pitch);

    return Normalize({
        std::sin(yaw) * cosPitch,
        std::sin(pitch),
        std::cos(yaw) * cosPitch
    });
}

} // namespace

Mat4 Multiply(
    const Mat4& a,
    const Mat4& b)
{
    Mat4 result{};

    for (int row = 0;
         row < 4;
         ++row) {
        for (int column = 0;
             column < 4;
             ++column) {
            float sum = 0.0f;

            for (int k = 0;
                 k < 4;
                 ++k) {
                sum +=
                    a.m[row * 4 + k] *
                    b.m[k * 4 + column];
            }

            result.m[
                row * 4 + column] =
                sum;
        }
    }

    return result;
}

void FreeCamera::Update(
    Platform::IWindow& window,
    float deltaSeconds)
{
    using Platform::Key;
    using Platform::MouseButton;

    if (window.IsMouseButtonDown(
            MouseButton::Right)) {
        const Platform::MouseDelta delta =
            window.ConsumeMouseDelta();

        yaw_ +=
            delta.x *
            mouseSensitivity;

        pitch_ -=
            delta.y *
            mouseSensitivity;

        pitch_ =
            std::clamp(
                pitch_,
                -1.50f,
                1.50f);
    } else {
        window.ConsumeMouseDelta();
    }

    const Vec3 forward =
        ForwardVector(
            yaw_,
            pitch_);

    const Vec3 worldUp{
        0.0f,
        1.0f,
        0.0f
    };

    const Vec3 right =
        Normalize(
            Cross(
                worldUp,
                forward));

    float speed =
        moveSpeed *
        deltaSeconds;

    if (window.IsKeyDown(
            Key::LeftShift)) {
        speed *=
            sprintMultiplier;
    }

    if (window.IsKeyDown(Key::W)) {
        position_.x +=
            forward.x * speed;
        position_.y +=
            forward.y * speed;
        position_.z +=
            forward.z * speed;
    }

    if (window.IsKeyDown(Key::S)) {
        position_.x -=
            forward.x * speed;
        position_.y -=
            forward.y * speed;
        position_.z -=
            forward.z * speed;
    }

    if (window.IsKeyDown(Key::D)) {
        position_.x +=
            right.x * speed;
        position_.z +=
            right.z * speed;
    }

    if (window.IsKeyDown(Key::A)) {
        position_.x -=
            right.x * speed;
        position_.z -=
            right.z * speed;
    }

    if (window.IsKeyDown(Key::E))
        position_.y += speed;

    if (window.IsKeyDown(Key::Q))
        position_.y -= speed;
}

void FreeCamera::MoveLocal(
    float forwardAmount,
    float rightAmount,
    float upAmount) noexcept
{
    const Vec3 forward =
        ForwardVector(
            yaw_,
            pitch_);

    const Vec3 worldUp{
        0.0f,
        1.0f,
        0.0f
    };

    const Vec3 right =
        Normalize(
            Cross(
                worldUp,
                forward));

    position_.x +=
        forward.x * forwardAmount +
        right.x * rightAmount;

    position_.y +=
        forward.y * forwardAmount +
        upAmount;

    position_.z +=
        forward.z * forwardAmount +
        right.z * rightAmount;
}

void FreeCamera::Rotate(
    float yawDelta,
    float pitchDelta) noexcept
{
    yaw_ += yawDelta;

    pitch_ =
        std::clamp(
            pitch_ + pitchDelta,
            -1.50f,
            1.50f);
}

Mat4 FreeCamera::ViewMatrix() const
{
    const Vec3 zAxis =
        ForwardVector(
            yaw_,
            pitch_);

    const Vec3 worldUp{
        0.0f,
        1.0f,
        0.0f
    };

    const Vec3 xAxis =
        Normalize(
            Cross(
                worldUp,
                zAxis));

    const Vec3 yAxis =
        Cross(
            zAxis,
            xAxis);

    Mat4 result{};

    result.m[0] = xAxis.x;
    result.m[1] = yAxis.x;
    result.m[2] = zAxis.x;

    result.m[4] = xAxis.y;
    result.m[5] = yAxis.y;
    result.m[6] = zAxis.y;

    result.m[8] = xAxis.z;
    result.m[9] = yAxis.z;
    result.m[10] = zAxis.z;

    result.m[12] =
        -Dot(xAxis, position_);
    result.m[13] =
        -Dot(yAxis, position_);
    result.m[14] =
        -Dot(zAxis, position_);
    result.m[15] = 1.0f;

    return result;
}

Mat4 FreeCamera::ProjectionMatrix(
    float aspectRatio) const
{
    Mat4 result{};

    const float yScale =
        1.0f /
        std::tan(
            verticalFovRadians *
            0.5f);

    const float xScale =
        yScale /
        std::max(
            aspectRatio,
            0.001f);

    const float zScale =
        farPlane /
        (farPlane - nearPlane);

    result.m[0] = xScale;
    result.m[5] = yScale;
    result.m[10] = zScale;
    result.m[11] = 1.0f;
    result.m[14] =
        -nearPlane *
        zScale;

    return result;
}

Mat4 FreeCamera::ViewProjection(
    float aspectRatio) const
{
    return Multiply(
        ViewMatrix(),
        ProjectionMatrix(
            aspectRatio));
}

} // namespace Hamun::Renderer
