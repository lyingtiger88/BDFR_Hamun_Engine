#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <Hamun/Assets/GltfAsset.hpp>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>

namespace Hamun::Assets {
namespace {

const cgltf_accessor* FindAttribute(
    const cgltf_primitive& primitive,
    cgltf_attribute_type type,
    cgltf_int index = 0)
{
    for (cgltf_size i = 0;
         i < primitive.attributes_count;
         ++i) {
        const cgltf_attribute& attribute =
            primitive.attributes[i];

        if (attribute.type == type &&
            attribute.index == index) {
            return attribute.data;
        }
    }

    return nullptr;
}

std::filesystem::path BaseColorTexturePath(
    const cgltf_primitive& primitive,
    const std::filesystem::path& sourcePath)
{
    if (!primitive.material ||
        !primitive.material->has_pbr_metallic_roughness)
        return {};

    const cgltf_texture_view& view =
        primitive.material
            ->pbr_metallic_roughness
            .base_color_texture;

    if (!view.texture ||
        !view.texture->image ||
        !view.texture->image->uri)
        return {};

    const std::string uri =
        view.texture->image->uri;

    if (uri.rfind("data:", 0) == 0)
        return {};

    return sourcePath.parent_path() /
        std::filesystem::path(uri);
}

void ReadBaseColorFactor(
    const cgltf_primitive& primitive,
    std::array<float, 4>& factor)
{
    if (!primitive.material ||
        !primitive.material->has_pbr_metallic_roughness)
        return;

    const auto& source =
        primitive.material
            ->pbr_metallic_roughness
            .base_color_factor;

    std::copy(
        std::begin(source),
        std::end(source),
        factor.begin());
}

} // namespace

std::optional<GltfAsset> LoadGltf(
    const std::filesystem::path& path,
    std::string* error)
{
    cgltf_options options{};
    cgltf_data* data = nullptr;

    const std::string pathString =
        path.string();

    cgltf_result result =
        cgltf_parse_file(
            &options,
            pathString.c_str(),
            &data);

    if (result != cgltf_result_success) {
        if (error)
            *error = "cgltf could not parse the glTF file";
        return std::nullopt;
    }

    result =
        cgltf_load_buffers(
            &options,
            data,
            pathString.c_str());

    if (result != cgltf_result_success) {
        if (error)
            *error = "cgltf could not load glTF buffers";
        cgltf_free(data);
        return std::nullopt;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        if (error)
            *error = "glTF validation failed";
        cgltf_free(data);
        return std::nullopt;
    }

    GltfAsset asset;

    for (cgltf_size meshIndex = 0;
         meshIndex < data->meshes_count;
         ++meshIndex) {
        const cgltf_mesh& sourceMesh =
            data->meshes[meshIndex];

        for (cgltf_size primitiveIndex = 0;
             primitiveIndex < sourceMesh.primitives_count;
             ++primitiveIndex) {
            const cgltf_primitive& primitive =
                sourceMesh.primitives[primitiveIndex];

            if (primitive.type !=
                cgltf_primitive_type_triangles)
                continue;

            const cgltf_accessor* positions =
                FindAttribute(
                    primitive,
                    cgltf_attribute_type_position);

            if (!positions ||
                positions->count == 0)
                continue;

            const cgltf_accessor* normals =
                FindAttribute(
                    primitive,
                    cgltf_attribute_type_normal);

            const cgltf_accessor* texcoords =
                FindAttribute(
                    primitive,
                    cgltf_attribute_type_texcoord,
                    0);

            MeshAsset mesh;

            if (sourceMesh.name)
                mesh.name = sourceMesh.name;

            if (sourceMesh.primitives_count > 1) {
                mesh.name +=
                    "_primitive_" +
                    std::to_string(primitiveIndex);
            }

            mesh.vertices.resize(
                static_cast<std::size_t>(
                    positions->count));

            for (cgltf_size vertexIndex = 0;
                 vertexIndex < positions->count;
                 ++vertexIndex) {
                MeshVertex& vertex =
                    mesh.vertices[
                        static_cast<std::size_t>(
                            vertexIndex)];

                cgltf_accessor_read_float(
                    positions,
                    vertexIndex,
                    vertex.position,
                    3);

                if (normals) {
                    cgltf_accessor_read_float(
                        normals,
                        vertexIndex,
                        vertex.normal,
                        3);
                }

                if (texcoords) {
                    cgltf_accessor_read_float(
                        texcoords,
                        vertexIndex,
                        vertex.uv,
                        2);
                }
            }

            if (primitive.indices) {
                mesh.indices.resize(
                    static_cast<std::size_t>(
                        primitive.indices->count));

                for (cgltf_size index = 0;
                     index < primitive.indices->count;
                     ++index) {
                    mesh.indices[
                        static_cast<std::size_t>(index)] =
                        static_cast<std::uint32_t>(
                            cgltf_accessor_read_index(
                                primitive.indices,
                                index));
                }
            } else {
                mesh.indices.resize(
                    mesh.vertices.size());

                for (std::size_t index = 0;
                     index < mesh.vertices.size();
                     ++index) {
                    mesh.indices[index] =
                        static_cast<std::uint32_t>(
                            index);
                }
            }

            mesh.baseColorTexture =
                BaseColorTexturePath(
                    primitive,
                    path);

            ReadBaseColorFactor(
                primitive,
                mesh.baseColorFactor);

            asset.meshes.push_back(
                std::move(mesh));
        }
    }

    cgltf_free(data);

    if (asset.meshes.empty()) {
        if (error)
            *error = "glTF contains no supported triangle meshes";
        return std::nullopt;
    }

    return asset;
}

} // namespace Hamun::Assets
