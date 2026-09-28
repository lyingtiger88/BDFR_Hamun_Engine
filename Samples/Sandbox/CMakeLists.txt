add_executable(HamunSandbox main.cpp)
target_link_libraries(HamunSandbox PRIVATE
  HamunCore
  HamunPlatform
  HamunAssets
  HamunRHI
  HamunRenderer
  HamunWorld
  HamunGraph
)


target_compile_definitions(HamunSandbox PRIVATE
  HAMUN_SANDBOX_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets"
)
