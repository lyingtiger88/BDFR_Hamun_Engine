add_library(HamunWorld STATIC
  src/StreamingScheduler.cpp
  src/World.cpp
)
target_include_directories(HamunWorld PUBLIC include)
target_link_libraries(HamunWorld PUBLIC HamunCore)
target_compile_features(HamunWorld PUBLIC cxx_std_20)
