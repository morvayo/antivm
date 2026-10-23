cmake_minimum_required(VERSION 3.20)
project(antivm LANGUAGES CXX)

add_library(antivm INTERFACE)
target_include_directories(antivm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(antivm INTERFACE cxx_std_17)
