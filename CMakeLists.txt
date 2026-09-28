cmake_minimum_required(VERSION 3.22.1)
project(BDFR_Hamun_Engine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HAMUN_BUILD_SANDBOX "Build Hamun Engine sandbox" ON)
option(HAMUN_BUILD_ANDROID_GAME "Build Hamun Android game bootstrap" OFF)
option(HAMUN_ENABLE_D3D12 "Enable Direct3D 12 backend" ${WIN32})
option(HAMUN_ENABLE_D3D11 "Enable Direct3D 11 compatibility backend" ${WIN32})
option(HAMUN_ENABLE_VULKAN "Enable Vulkan backend foundation" ON)
option(HAMUN_ENABLE_GLES "Enable OpenGL ES backend foundation" ON)

add_subdirectory(Engine/HamunCore)
add_subdirectory(Engine/HamunPlatform)
add_subdirectory(Engine/HamunRHI)
add_subdirectory(Engine/HamunWorld)
add_subdirectory(Engine/HamunGraph)

if(HAMUN_BUILD_SANDBOX)
  add_subdirectory(Samples/Sandbox)
endif()

if(ANDROID AND HAMUN_BUILD_ANDROID_GAME)
  add_subdirectory(Platforms/Android/native)
endif()
