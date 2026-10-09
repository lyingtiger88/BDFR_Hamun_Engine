#include "../src/EditorSceneDocument.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() /
    ("hamun-scene-save-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    fs::create_directories(root);
    const auto asset = root / "test.gltf";
    { std::ofstream stream(asset); stream << "{}"; }
    const auto path = root / "level.hamunscene";
    Hamun::Editor::SceneDocument doc;
    doc.sourceAsset = asset;
    Hamun::Editor::SceneObjectState object;
    object.name = "Before";
    doc.objects.push_back(object);
    std::string error;
    if (!Hamun::Editor::SaveSceneDocument(path, doc, &error)) throw std::runtime_error(error);
    doc.objects[0].name = "After";
    if (!Hamun::Editor::SaveSceneDocument(path, doc, &error)) throw std::runtime_error(error);
    Hamun::Editor::SceneDocument loaded;
    if (!Hamun::Editor::LoadSceneDocument(path, loaded, &error)) throw std::runtime_error(error);
    if (loaded.objects.size()!=1 || loaded.objects[0].name!="After")
      throw std::runtime_error("Round-trip overwrite failed");
    for (const auto& file : fs::directory_iterator(root)) {
      if (file.path().filename().string().find(".tmp-")!=std::string::npos ||
          file.path().filename().string().find(".bak-")!=std::string::npos)
        throw std::runtime_error("Temporary file was not cleaned");
    }
    fs::remove_all(root);
    std::cout << "Editor scene persistence tests passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    fs::remove_all(root);
    return 1;
  }
}
