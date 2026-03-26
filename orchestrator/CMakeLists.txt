find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

set(ORCHESTRATOR_SOURCES
    src/containers/ContainerEngineFactory.cpp
    src/containers/internal/DockerClient.cpp
    src/interfaces/cli/CliParser.cpp
    src/interfaces/web/Server.cpp
    src/manifests/ManifestParser.cpp
    src/perturbations/PerturbationFactory.cpp
    src/perturbations/internal/KillPerturbation.cpp
    src/perturbations/internal/MemoryCapPerturbation.cpp
    src/perturbations/internal/CpuCapPerturbation.cpp
    src/perturbations/internal/NetworkDelayPerturbation.cpp
    src/perturbations/internal/NetworkCutoffPerturbation.cpp
    src/perturbations/internal/GarbagePacketPerturbation.cpp
    src/observability/ObservabilityEngine.cpp
    src/observability/ValidationEngine.cpp
)

add_library(orchestrator_core STATIC ${ORCHESTRATOR_SOURCES})

target_include_directories(orchestrator_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(orchestrator_core PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
    fmt::fmt
)

if(UNIX)
    target_compile_options(orchestrator_core PRIVATE
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
    )
endif()

add_executable(orchestrator_app src/main.cpp)
target_include_directories(orchestrator_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(orchestrator_app PRIVATE orchestrator_core)
set_target_properties(orchestrator_app PROPERTIES OUTPUT_NAME chaos)

add_executable(orchestrator_unit_tests 
    tests/containers/internal/DockerClientUnitTest.cpp
    tests/manifests/ManifestParserUnitTest.cpp
    tests/perturbations/PerturbationTests.cpp
    tests/observability/ObservabilityEngineUnitTest.cpp
    tests/observability/ValidationEngineUnitTest.cpp
)

target_include_directories(orchestrator_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/perturbations
)
target_link_libraries(orchestrator_unit_tests PRIVATE
    orchestrator_core
    GTest::gmock
    GTest::gtest_main
)

add_executable(orchestrator_smoke_tests tests/smoke/DockerClientIntegrationTest.cpp)
target_link_libraries(orchestrator_smoke_tests PRIVATE
    orchestrator_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(orchestrator_unit_tests)
gtest_discover_tests(orchestrator_smoke_tests)
