add_library(HamunRenderer STATIC
  src/FreeCamera.cpp
  src/Material.cpp
  src/Renderer.cpp
)

target_include_directories(HamunRenderer PUBLIC include)
target_link_libraries(HamunRenderer
  PUBLIC
    HamunCore
    HamunPlatform
    HamunGraph
)
target_compile_features(HamunRenderer PUBLIC cxx_std_20)
