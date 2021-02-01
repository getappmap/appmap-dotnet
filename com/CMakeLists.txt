cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(com LANGUAGES CXX)

file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_library(com INTERFACE)
set_target_properties(com PROPERTIES INTERFACE_COMPILE_FEATURES cxx_std_20)

target_link_libraries(com INTERFACE pal)
target_include_directories(pal INTERFACE include)
