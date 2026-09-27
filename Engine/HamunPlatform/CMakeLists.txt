add_library(HamunPlatform STATIC)

target_include_directories(HamunPlatform PUBLIC include)
target_link_libraries(HamunPlatform PUBLIC HamunCore)
target_compile_features(HamunPlatform PUBLIC cxx_std_20)

if(WIN32)
  target_sources(HamunPlatform PRIVATE src/Win32/Win32Window.cpp)
  target_compile_definitions(HamunPlatform PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
  )
  target_link_libraries(HamunPlatform PRIVATE user32 gdi32)
else()
  target_sources(HamunPlatform PRIVATE src/WindowStub.cpp)
endif()
