cmake_minimum_required(VERSION 3.20)
project(anvil_sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(anvil_sample src/main.cpp src/engine.cpp)
target_include_directories(anvil_sample PRIVATE include)
