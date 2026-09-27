if(NOT ANDROID)
  return()
endif()

set(HAMUN_ANDROID_NATIVE_APP_GLUE
    "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(HamunNativeAppGlue STATIC
  "${HAMUN_ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c"
)

target_include_directories(HamunNativeAppGlue PUBLIC
  "${HAMUN_ANDROID_NATIVE_APP_GLUE}"
)

add_library(HamunAndroidGame SHARED
  AndroidMain.cpp
)

target_compile_features(HamunAndroidGame PRIVATE cxx_std_20)

target_link_libraries(HamunAndroidGame PRIVATE
  HamunCore
  HamunRHI
  HamunWorld
  HamunGraph
  HamunNativeAppGlue
  android
  log
)

target_link_options(HamunAndroidGame PRIVATE
  "-Wl,-u,ANativeActivity_onCreate"
)
