#include <Hamun/Hair/TfxAsset.hpp>

#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace Hamun::Hair {
namespace {

struct TfxFileHeader {
    float version = 0.0f;
    std::uint32_t numHairStrands = 0;
    std::uint32_t numVerticesPerStrand = 0;
    std::uint32_t offsetVertexPosition = 0;
    std::uint32_t offsetStrandUV = 0;
    std::uint32_t offsetVertexUV = 0;
    std::uint32_t offsetStrandThickness = 0;
    std::uint32_t offsetVertexColor = 0;
    std::uint32_t reserved[32]{};
};

static_assert(
    sizeof(TfxFileHeader) == 160,
    "TressFX .tfx header layout changed");

bool CheckedRange(
    std::size_t offset,
    std::size_t byteCount,
    std::size_t totalSize) noexcept
{
    return
        offset <= totalSize &&
        byteCount <= totalSize - offset;
}

void SetError(
    std::string* error,
    std::string message)
{
    if (error)
        *error = std::move(message);
}

} // namespace

std::optional<TfxAsset> LoadTfx(
    std::span<const std::byte> bytes,
    std::string* error)
{
    if (bytes.size() <
        sizeof(TfxFileHeader)) {
        SetError(
            error,
            "TressFX file is smaller than the .tfx header.");
        return std::nullopt;
    }

    TfxFileHeader header{};
    std::memcpy(
        &header,
        bytes.data(),
        sizeof(header));

    if (header.version < 4.0f) {
        SetError(
            error,
            "Unsupported TressFX asset version; Hamun requires .tfx version 4.x or newer.");
        return std::nullopt;
    }

    if (header.numHairStrands == 0) {
        SetError(
            error,
            "TressFX asset contains no guide strands.");
        return std::nullopt;
    }

    const std::uint32_t verticesPerStrand =
        header.numVerticesPerStrand;

    const bool validVertexCount =
        verticesPerStrand >= 4u &&
        verticesPerStrand <= 64u &&
        (verticesPerStrand &
         (verticesPerStrand - 1u)) == 0u;

    if (!validVertexCount) {
        SetError(
            error,
            "TressFX vertices-per-strand must be a power of two in the range 4..64.");
        return std::nullopt;
    }

    const std::uint64_t totalVertices64 =
        static_cast<std::uint64_t>(
            header.numHairStrands) *
        static_cast<std::uint64_t>(
            verticesPerStrand);

    if (totalVertices64 >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        SetError(
            error,
            "TressFX vertex count overflows this platform.");
        return std::nullopt;
    }

    const std::size_t totalVertices =
        static_cast<std::size_t>(
            totalVertices64);

    const std::size_t positionBytes =
        totalVertices *
        sizeof(std::array<float, 4>);

    if (!CheckedRange(
            header.offsetVertexPosition,
            positionBytes,
            bytes.size())) {
        SetError(
            error,
            "TressFX position stream is outside the file bounds.");
        return std::nullopt;
    }

    TfxAsset asset;
    asset.version =
        header.version;
    asset.guideStrandCount =
        header.numHairStrands;
    asset.verticesPerStrand =
        verticesPerStrand;

    asset.positions.resize(
        totalVertices);

    std::memcpy(
        asset.positions.data(),
        bytes.data() +
            header.offsetVertexPosition,
        positionBytes);

    if (header.offsetStrandUV != 0) {
        const std::size_t uvBytes =
            static_cast<std::size_t>(
                header.numHairStrands) *
            sizeof(std::array<float, 2>);

        if (!CheckedRange(
                header.offsetStrandUV,
                uvBytes,
                bytes.size())) {
            SetError(
                error,
                "TressFX strand UV stream is outside the file bounds.");
            return std::nullopt;
        }

        asset.strandUv.resize(
            header.numHairStrands);

        std::memcpy(
            asset.strandUv.data(),
            bytes.data() +
                header.offsetStrandUV,
            uvBytes);
    }

    return asset;
}

std::optional<TfxAsset> LoadTfx(
    const std::filesystem::path& path,
    std::string* error)
{
    std::ifstream file(
        path,
        std::ios::binary |
        std::ios::ate);

    if (!file) {
        SetError(
            error,
            "Could not open TressFX .tfx file.");
        return std::nullopt;
    }

    const std::streamsize size =
        file.tellg();

    if (size <= 0) {
        SetError(
            error,
            "TressFX .tfx file is empty.");
        return std::nullopt;
    }

    file.seekg(
        0,
        std::ios::beg);

    std::vector<std::byte> bytes(
        static_cast<std::size_t>(
            size));

    if (!file.read(
            reinterpret_cast<char*>(
                bytes.data()),
            size)) {
        SetError(
            error,
            "Could not read TressFX .tfx file.");
        return std::nullopt;
    }

    return LoadTfx(
        bytes,
        error);
}

} // namespace Hamun::Hair
