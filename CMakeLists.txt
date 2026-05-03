cmake_minimum_required(VERSION 3.28)

project(chaos
    VERSION 0.3.0 
    LANGUAGES C CXX
    DESCRIPTION "Resilience Tool for Docker Containers"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(wrapper)
add_subdirectory(orchestrator)
