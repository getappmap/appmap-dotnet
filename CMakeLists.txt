cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(
  appmap-dotnet
  VERSION 0.0.1
  LANGUAGES CXX
)

include(CTest)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME utfcpp
    GITHUB_REPOSITORY nemtrif/utfcpp
    VERSION 3.1.2
)

CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.9.1
  # not using the repo as it takes forever to clone
  URL https://github.com/nlohmann/json/releases/download/v3.9.1/include.zip
)

CPMAddPackage(
  NAME spdlog
  VERSION 1.8.2
  GITHUB_REPOSITORY gabime/spdlog
)
target_compile_options(spdlog PRIVATE -fPIC)

if (nlohmann_json_ADDED)
  add_library(nlohmann_json INTERFACE IMPORTED)
  target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
endif()

CPMAddPackage(
  NAME doctest
  GITHUB_REPOSITORY onqtam/doctest
  GIT_TAG 2.4.5
)

CPMAddPackage(
  NAME yaml-cpp
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG yaml-cpp-0.6.3
  OPTIONS "YAML_CPP_BUILD_TESTS OFF;YAML_CPP_BUILD_TOOLS OFF;YAML_CPP_BUILD_CONTRIB OFF"
)

CPMAddPackage(
  NAME gsl-lite
  GITHUB_REPOSITORY gsl-lite/gsl-lite
  VERSION 0.37.0
)

CPMAddPackage(
  NAME range-v3
  GITHUB_REPOSITORY ericniebler/range-v3
  GIT_TAG 0.11.0
  VERSION 0.11.0
)

target_compile_options(yaml-cpp PRIVATE -fPIC)

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
target_include_directories(appmap-instrumentation INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)

set_target_properties(appmap-instrumentation PROPERTIES CXX_STANDARD 20)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(appmap-instrumentation PUBLIC "$<$<BOOL:${MSVC}>:/permissive->")
target_compile_options(appmap-instrumentation PRIVATE -Wno-pmf-conversions)

add_subdirectory(pal)
add_subdirectory(com)
add_subdirectory(clrie)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()

target_link_libraries(appmap-instrumentation
    PRIVATE com spdlog gsl-lite nlohmann_json yaml-cpp stdc++fs doctest range-v3
    PUBLIC clrie
)
