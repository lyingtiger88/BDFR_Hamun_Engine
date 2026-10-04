add_executable(HamunEditor WIN32
  src/main.cpp
)

target_compile_features(HamunEditor PRIVATE cxx_std_20)
target_link_libraries(HamunEditor PRIVATE
  HamunProject
  shell32
  comdlg32
)

if(MSVC)
  target_compile_definitions(HamunEditor PRIVATE UNICODE _UNICODE)
endif()

add_custom_command(
  TARGET HamunEditor
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/Templates"
          "$<TARGET_FILE_DIR:HamunEditor>/Templates"
)
