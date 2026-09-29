#pragma once

#include <array>
#include <memory>
#include <string>

namespace Hamun::Renderer {

struct MaterialParameters {
    std::array<float, 4> baseColorFactor{
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    float metallic = 0.0f;
    float roughness = 1.0f;
};

struct MaterialDesc {
    std::string name = "Material";
    MaterialParameters parameters{};
    bool depthTest = true;
};

class Material {
public:
    explicit Material(MaterialDesc desc);

    [[nodiscard]] const std::string& Name() const noexcept
    {
        return desc_.name;
    }

    [[nodiscard]] const MaterialParameters& Parameters() const noexcept
    {
        return desc_.parameters;
    }

    [[nodiscard]] bool DepthTest() const noexcept
    {
        return desc_.depthTest;
    }

private:
    MaterialDesc desc_;
};

class MaterialInstance {
public:
    explicit MaterialInstance(
        std::shared_ptr<const Material> baseMaterial);

    [[nodiscard]] const Material& BaseMaterial() const noexcept
    {
        return *baseMaterial_;
    }

    [[nodiscard]] const MaterialParameters& Parameters() const noexcept
    {
        return parameters_;
    }

    void SetBaseColorFactor(
        const std::array<float, 4>& value) noexcept
    {
        parameters_.baseColorFactor = value;
    }

    void SetMetallic(float value) noexcept
    {
        parameters_.metallic = value;
    }

    void SetRoughness(float value) noexcept
    {
        parameters_.roughness = value;
    }

private:
    std::shared_ptr<const Material> baseMaterial_;
    MaterialParameters parameters_{};
};

} // namespace Hamun::Renderer
