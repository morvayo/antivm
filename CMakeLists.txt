cmake_minimum_required(VERSION 3.20)
project(antivm LANGUAGES CXX)

add_library(antivm INTERFACE)
target_include_directories(antivm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(antivm INTERFACE cxx_std_17)

option(ANTIVM_BUILD_TESTS "Build tests" OFF)
if(ANTIVM_BUILD_TESTS)
    enable_testing()
    add_executable(antivm_cpu_test tests/cpu.cpp)
    target_link_libraries(antivm_cpu_test PRIVATE antivm)
    add_test(NAME cpu_interface COMMAND antivm_cpu_test)
endif()
