add_library(HamunHair STATIC
  src/HairGpuResources.cpp
  src/HairRuntime.cpp
  src/HairSimulation.cpp
  src/TfxAsset.cpp
  src/TressFxSdkBridge.cpp
)

target_include_directories(HamunHair PUBLIC include)

target_link_libraries(HamunHair
  PUBLIC
    HamunCore
    HamunRHI
    HamunRenderer
)

target_compile_features(HamunHair PUBLIC cxx_std_20)

target_compile_definitions(HamunHair PRIVATE
  $<$<BOOL:${HAMUN_WITH_TRESSFX_SDK}>:HAMUN_WITH_TRESSFX_SDK=1>
)


if(HAMUN_WITH_TRESSFX_SDK)
  if(NOT HAMUN_TRESSFX_SDK_DIR)
    message(FATAL_ERROR "HAMUN_WITH_TRESSFX_SDK=ON requires HAMUN_TRESSFX_SDK_DIR")
  endif()

  if(NOT EXISTS "${HAMUN_TRESSFX_SDK_DIR}/src/TressFX/AMD_TressFX.h")
    message(FATAL_ERROR "HAMUN_TRESSFX_SDK_DIR does not contain src/TressFX/AMD_TressFX.h")
  endif()

  target_include_directories(HamunHair PRIVATE
    "${HAMUN_TRESSFX_SDK_DIR}/src"
    "${HAMUN_TRESSFX_SDK_DIR}/src/TressFX"
  )
endif()
