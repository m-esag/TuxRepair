cmake_minimum_required(VERSION 3.16)
project(TuxRepair VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)        # Portable C++23, not g++-only extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to RelWithDebInfo so an unset CMAKE_BUILD_TYPE produces a sensible
# optimized+debuggable binary instead of an unoptimized no-debug build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Compiler warning / sanitizer settings shared by all targets.
include(${CMAKE_SOURCE_DIR}/cmake/compiler_warnings.cmake)

# Find Qt6 components. 6.5 is the current LTS reasonable baseline for the
# Widgets + PrintSupport surface this project uses.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets PrintSupport)

enable_testing()

add_subdirectory(core)
add_subdirectory(desktop-ui)
add_subdirectory(tests)
