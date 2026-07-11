cmake_minimum_required(VERSION 3.16)
project(TuxRepair VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

add_subdirectory(core)
add_subdirectory(desktop-ui)
