add_library(HamunUpscale STATIC
  src/FsrRuntime.cpp
)

target_include_directories(HamunUpscale PUBLIC include)

target_link_libraries(HamunUpscale
  PUBLIC
    HamunCore
    HamunRHI
)

target_compile_features(HamunUpscale PUBLIC cxx_std_20)

if(HAMUN_WITH_FSR_SDK)
  if(NOT HAMUN_FSR_SDK_DIR)
    message(FATAL_ERROR "HAMUN_WITH_FSR_SDK=ON requires HAMUN_FSR_SDK_DIR")
  endif()

  target_include_directories(HamunUpscale PRIVATE
    "${HAMUN_FSR_SDK_DIR}/Kits/FidelityFX/api/include"
    "${HAMUN_FSR_SDK_DIR}/Kits/FidelityFX/upscalers/include"
  )

  target_compile_definitions(HamunUpscale PRIVATE
    HAMUN_WITH_FSR_SDK=1
  )
endif()
