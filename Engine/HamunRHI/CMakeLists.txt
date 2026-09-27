add_library(HamunRHI STATIC
  src/RHI.cpp
  src/backends/D3D12/D3D12Backend.cpp
  src/backends/Vulkan/VulkanBackend.cpp
  src/backends/GLES/GLESBackend.cpp
)

target_include_directories(HamunRHI PUBLIC include)
target_link_libraries(HamunRHI PUBLIC HamunCore)
target_compile_features(HamunRHI PUBLIC cxx_std_20)

target_compile_definitions(HamunRHI PRIVATE
  $<$<BOOL:${HAMUN_ENABLE_D3D12}>:HAMUN_ENABLE_D3D12=1>
  $<$<BOOL:${HAMUN_ENABLE_VULKAN}>:HAMUN_ENABLE_VULKAN=1>
  $<$<BOOL:${HAMUN_ENABLE_GLES}>:HAMUN_ENABLE_GLES=1>
)

if(WIN32 AND HAMUN_ENABLE_D3D12)
  target_compile_definitions(HamunRHI PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(HamunRHI PRIVATE d3d12 dxgi d3dcompiler)
endif()
