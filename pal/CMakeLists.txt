cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(pal LANGUAGES CXX)

add_library(pal source/guid.cpp source/bstr.cpp)

target_compile_options(pal PRIVATE -fPIC)
target_compile_definitions(pal PUBLIC PAL_STDCPP_COMPAT)
target_compile_definitions(pal PUBLIC TARGET_AMD64)
target_include_directories(pal SYSTEM PUBLIC include)
