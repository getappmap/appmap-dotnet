cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  appmap-dotnet
  VERSION 0.0.1
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(Greeter INTERFACE) set_target_properties(Greeter PROPERTIES
# INTERFACE_COMPILE_FEATURES cxx_std_17)

add_library(appmap-instrumentation SHARED ${headers} ${sources})

set_target_properties(appmap-instrumentation PROPERTIES CXX_STANDARD 20)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(appmap-instrumentation PUBLIC "$<$<BOOL:${MSVC}>:/permissive->")

target_compile_options(appmap-instrumentation PRIVATE -W -Wall -Werror -pedantic)

add_subdirectory(pal)
add_subdirectory(com)
target_link_libraries(appmap-instrumentation
    PRIVATE com
)
