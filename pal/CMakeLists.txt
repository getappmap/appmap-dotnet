cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(pal LANGUAGES CXX)

add_library(pal INTERFACE)

target_compile_definitions(pal INTERFACE PAL_STDCPP_COMPAT)
target_include_directories(pal SYSTEM INTERFACE include)
