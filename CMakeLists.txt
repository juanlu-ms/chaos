cmake_minimum_required(VERSION 3.28)

project(chaos
    VERSION 0.1.0 
    LANGUAGES CXX
    DESCRIPTION "Resilience Tool for Docker Containers"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

set(CORE_SOURCES
    src/adapters/infra/docker/DockerClientAdapter.cpp
    src/adapters/ui/cli/CliAdapter.cpp
    src/adapters/ui/web/Server.cpp
)

add_library(chaos_core STATIC ${CORE_SOURCES})
target_include_directories(chaos_core PUBLIC 
    ${CMAKE_SOURCE_DIR}/src
)


target_link_libraries(chaos_core PUBLIC 
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
    fmt::fmt
)

if(UNIX)
    target_compile_options(chaos_core PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
    )
endif()

add_executable(chaos src/main.cpp)
target_link_libraries(chaos PRIVATE chaos_core)
enable_testing()

add_executable(chaos_tests 
    tests/adapters/infra/docker/DockerClientAdapterUnitTest.cpp
    tests/adapters/infra/docker/DockerClientAdapterTest.cpp
)

target_link_libraries(chaos_tests PRIVATE 
    chaos_core
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(chaos_tests)
