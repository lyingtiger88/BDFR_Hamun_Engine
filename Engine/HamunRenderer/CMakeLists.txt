add_library(HamunRenderer STATIC
  src/ComputePass.cpp
  src/FreeCamera.cpp
  src/FrameResources.cpp
  src/Material.cpp
  src/MainRenderPass.cpp
  src/RenderGraph.cpp
  src/Renderer.cpp
)

target_include_directories(HamunRenderer PUBLIC include)
target_link_libraries(HamunRenderer
  PUBLIC
    HamunCore
    HamunPlatform
    HamunRHI
)
target_compile_features(HamunRenderer PUBLIC cxx_std_20)
