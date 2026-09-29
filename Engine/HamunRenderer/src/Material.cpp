#include <Hamun/Renderer/Material.hpp>

#include <stdexcept>
#include <utility>

namespace Hamun::Renderer {

Material::Material(MaterialDesc desc)
    : desc_(std::move(desc))
{
}

MaterialInstance::MaterialInstance(
    std::shared_ptr<const Material> baseMaterial)
    : baseMaterial_(std::move(baseMaterial))
{
    if (!baseMaterial_) {
        throw std::invalid_argument(
            "HamunRenderer: MaterialInstance requires a base material");
    }

    parameters_ =
        baseMaterial_->Parameters();
}

} // namespace Hamun::Renderer
