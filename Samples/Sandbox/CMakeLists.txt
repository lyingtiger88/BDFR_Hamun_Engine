add_executable(HamunSandbox main.cpp)
target_link_libraries(HamunSandbox PRIVATE
  HamunCore
  HamunPlatform
  HamunAssets
  HamunRHI
  HamunRenderer
  HamunWorld
  HamunGraph
  HamunHair
  HamunUpscale
)

add_custom_command(TARGET HamunSandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
          "$<TARGET_FILE_DIR:HamunSandbox>/Assets"
  COMMENT "Copying Hamun Sandbox assets next to executable"
)
