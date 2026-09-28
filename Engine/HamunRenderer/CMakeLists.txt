add_library(HamunRenderer STATIC
  src/FreeCamera.cpp
)

target_include_directories(HamunRenderer PUBLIC include)
target_link_libraries(HamunRenderer
  PUBLIC
    HamunCore
    HamunPlatform
)
target_compile_features(HamunRenderer PUBLIC cxx_std_20)
