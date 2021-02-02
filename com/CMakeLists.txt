cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(com LANGUAGES CXX)

file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_library(com source/hresult.cpp)
set_target_properties(com PROPERTIES CXX_STANDARD 20)
target_compile_options(com PRIVATE -fPIC)

target_link_libraries(com PUBLIC pal)
target_link_libraries(com PUBLIC utf8::cpp)
target_include_directories(com PUBLIC include)
