cmake_minimum_required(VERSION 3.24)
project(HamunRuntime LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "Hamun Runtime v0.1 currently targets Windows x64.")
endif()
find_package(Vulkan REQUIRED)
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
add_executable(HamunRuntime WIN32 src/main.cpp)
target_compile_features(HamunRuntime PRIVATE cxx_std_17)
target_compile_definitions(HamunRuntime PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX VK_USE_PLATFORM_WIN32_KHR)
target_link_libraries(HamunRuntime PRIVATE Vulkan::Vulkan user32 gdi32 psapi shell32)
set(SHADER_OUTPUTS)
foreach(name triangle.vert triangle.frag)
  set(out "${CMAKE_CURRENT_BINARY_DIR}/shaders/${name}.spv")
  add_custom_command(OUTPUT "${out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND "${GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}" -o "${out}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}" VERBATIM)
  list(APPEND SHADER_OUTPUTS "${out}")
endforeach()
add_custom_target(HamunShaders DEPENDS ${SHADER_OUTPUTS})
add_dependencies(HamunRuntime HamunShaders)
add_custom_command(TARGET HamunRuntime POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:HamunRuntime>/shaders")
