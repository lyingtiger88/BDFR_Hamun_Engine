cmake_minimum_required(VERSION 3.22.1)
project({{PROJECT_NAME}} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable({{PROJECT_NAME}}
  Source/Main.cpp
)
