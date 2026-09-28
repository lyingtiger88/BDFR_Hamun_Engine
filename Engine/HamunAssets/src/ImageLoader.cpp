#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#include <Hamun/Assets/ImageAsset.hpp>

namespace Hamun::Assets {

std::optional<ImageAsset> LoadImageRGBA8(
    const std::filesystem::path& path,
    std::string* error)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;

    stbi_uc* pixels = stbi_load(
        path.string().c_str(),
        &width,
        &height,
        &sourceChannels,
        STBI_rgb_alpha);

    if (!pixels) {
        if (error) {
            const char* reason =
                stbi_failure_reason();
            *error =
                reason
                    ? reason
                    : "stb_image failed to load image";
        }
        return std::nullopt;
    }

    ImageAsset image;
    image.width =
        static_cast<std::uint32_t>(width);
    image.height =
        static_cast<std::uint32_t>(height);

    const std::size_t byteCount =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) *
        4u;

    image.rgba8.assign(
        pixels,
        pixels + byteCount);

    stbi_image_free(pixels);
    return image;
}

} // namespace Hamun::Assets
