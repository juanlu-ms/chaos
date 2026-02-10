cmake_minimum_required(VERSION 3.28)

project(kaos
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

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/main.cpp$")

add_library(kaos_core STATIC ${CORE_SOURCES})

target_include_directories(kaos_core PUBLIC 
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(kaos_core PUBLIC 
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
)

if(UNIX)
    target_compile_options(kaos_core PRIVATE 
        -Wall -Wextra -Wpedantic 
        -Wconversion -Wshadow
    )
endif()

add_executable(kaos src/main.cpp)
target_link_libraries(kaos PRIVATE kaos_core)

enable_testing()

add_executable(kaos_tests 
    tests/adapters/infra/docker/DockerClientAdapterTest.cpp
    src/adapters/infra/docker/DockerClientAdapter.cpp
)

target_link_libraries(kaos_tests PRIVATE 
    kaos_core
    GTest::gtest_main
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    httplib::httplib
)

include(GoogleTest)
gtest_discover_tests(kaos_tests)
