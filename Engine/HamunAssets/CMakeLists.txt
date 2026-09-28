add_library(HamunAssets STATIC
  src/ImageLoader.cpp
  src/GltfImporter.cpp
)

target_include_directories(HamunAssets
  PUBLIC include
  PRIVATE
    ${PROJECT_SOURCE_DIR}/ThirdParty/stb
    ${PROJECT_SOURCE_DIR}/ThirdParty/cgltf
)

target_link_libraries(HamunAssets PUBLIC HamunCore)
target_compile_features(HamunAssets PUBLIC cxx_std_20)
