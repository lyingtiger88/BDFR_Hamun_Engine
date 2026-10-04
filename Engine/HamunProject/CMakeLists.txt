add_library(HamunProject STATIC
  src/ProjectCreator.cpp
  src/TemplateCatalog.cpp
)

target_include_directories(HamunProject PUBLIC include)
target_compile_features(HamunProject PUBLIC cxx_std_20)
