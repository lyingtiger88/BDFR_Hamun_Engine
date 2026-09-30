add_library(HamunHair STATIC
  src/HairRuntime.cpp
  src/TfxAsset.cpp
)

target_include_directories(HamunHair PUBLIC include)

target_link_libraries(HamunHair
  PUBLIC
    HamunCore
    HamunRHI
)

target_compile_features(HamunHair PUBLIC cxx_std_20)

target_compile_definitions(HamunHair PRIVATE
  $<$<BOOL:${HAMUN_WITH_TRESSFX_SDK}>:HAMUN_WITH_TRESSFX_SDK=1>
)
