add_library(HamunGraph STATIC
  src/VM.cpp
)

target_include_directories(HamunGraph PUBLIC include)
target_link_libraries(HamunGraph PUBLIC HamunCore)
target_compile_features(HamunGraph PUBLIC cxx_std_20)

include(CTest)
if(BUILD_TESTING)
  add_executable(HamunGraphVMTests tests/VMTests.cpp)
  target_link_libraries(HamunGraphVMTests PRIVATE HamunGraph)
  add_test(NAME HamunGraphVMTests COMMAND HamunGraphVMTests)
endif()
