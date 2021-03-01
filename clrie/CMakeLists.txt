cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(clrie LANGUAGES CXX)

add_library(clrie InstrumentationEngine.Api/InstrumentationEngine_i.cpp)
target_compile_options(clrie PRIVATE -Wno-attributes)

set_target_properties(clrie PROPERTIES CXX_STANDARD 20)
target_include_directories(clrie PUBLIC SYSTEM InstrumentationEngine.Api)
target_include_directories(clrie INTERFACE include)
target_link_libraries(clrie PUBLIC pal com)
