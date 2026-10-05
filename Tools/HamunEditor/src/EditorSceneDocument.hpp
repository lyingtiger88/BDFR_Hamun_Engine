#pragma once

#include "EditorScenePreview.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Hamun::Editor {

struct SceneDocument {
    std::filesystem::path sourceAsset;
    std::vector<SceneObjectTransform> transforms;

    bool hasCamera = false;
    Renderer::Vec3 cameraPosition{};
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
};

bool SaveSceneDocument(
    const std::filesystem::path& path,
    const SceneDocument& document,
    std::string* error = nullptr);

bool LoadSceneDocument(
    const std::filesystem::path& path,
    SceneDocument& document,
    std::string* error = nullptr);

} // namespace Hamun::Editor
