cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(
  appmap-dotnet
  VERSION 0.0.3
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

CPMAddPackage("gh:nemtrif/utfcpp@3.1.2")

CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.9.1
  # not using the repo as it takes forever to clone
  URL https://github.com/nlohmann/json/releases/download/v3.9.1/include.zip
  OPTIONS "JSON_BuildTests OFF"
)

if (nlohmann_json_ADDED)
  add_library(nlohmann_json INTERFACE IMPORTED)
  target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
endif()

CPMAddPackage("gh:gabime/spdlog@1.8.2")
target_compile_options(spdlog PRIVATE -fPIC)

CPMAddPackage("gh:onqtam/doctest@2.4.5#2.4.5")

CPMAddPackage(
  NAME yaml-cpp
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG yaml-cpp-0.6.3
  VERSION 0.6.3
  OPTIONS "YAML_CPP_BUILD_TESTS OFF;YAML_CPP_BUILD_TOOLS OFF;YAML_CPP_BUILD_CONTRIB OFF"
)
target_compile_options(yaml-cpp PRIVATE -fPIC)

CPMAddPackage("gh:gsl-lite/gsl-lite@0.37.0")

CPMAddPackage("gh:ericniebler/range-v3#0.11.0@0.11.0")


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
target_compile_options(appmap-instrumentation PRIVATE -Wno-pmf-conversions -fconcepts)

add_subdirectory(pal)
add_subdirectory(com)
add_subdirectory(clrie)

if (BUILD_TESTING)
  add_subdirectory(test)
else()
  target_compile_definitions(appmap-instrumentation PRIVATE DOCTEST_CONFIG_DISABLE)
endif()

target_link_libraries(appmap-instrumentation
    PRIVATE com spdlog gsl-lite nlohmann_json yaml-cpp stdc++fs doctest range-v3
    PUBLIC clrie
)
