add_executable(HamunSandbox main.cpp)
target_link_libraries(HamunSandbox PRIVATE
  HamunCore
  HamunPlatform
  HamunRHI
  HamunWorld
  HamunGraph
)
