add_executable(HamunLauncher WIN32
  src/main.cpp
)

target_compile_features(HamunLauncher PRIVATE cxx_std_20)
target_link_libraries(HamunLauncher PRIVATE
  HamunProject
  shell32
  ole32
)

if(MSVC)
  target_compile_definitions(HamunLauncher PRIVATE UNICODE _UNICODE)
endif()

add_custom_command(
  TARGET HamunLauncher
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/Templates"
          "$<TARGET_FILE_DIR:HamunLauncher>/Templates"
)
