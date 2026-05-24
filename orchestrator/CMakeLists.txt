find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

add_library(chaos_compiler_flags INTERFACE)
if(UNIX)
    target_compile_options(chaos_compiler_flags INTERFACE
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
    )
endif()

set(ORCHESTRATOR_SOURCES
    src/containers/ContainerEngineFactory.cpp
    src/containers/internal/DockerClient.cpp
    src/containers/internal/CgroupMetricsGatherer.cpp
    src/interfaces/cli/CliParser.cpp
    src/interfaces/web/Server.cpp
    src/manifests/ManifestParser.cpp
    src/perturbations/PerturbationEngine.cpp
    src/perturbations/PerturbationFactory.cpp
    src/perturbations/internal/KillPerturbation.cpp
    src/perturbations/internal/MemoryCapPerturbation.cpp
    src/perturbations/internal/CpuCapPerturbation.cpp
    src/perturbations/internal/NetworkDelayPerturbation.cpp
    src/perturbations/internal/NetworkCutoffPerturbation.cpp
    src/perturbations/internal/PacketFloodPerturbation.cpp
    src/perturbations/internal/TrafficCorruptionPerturbation.cpp
    src/observability/ObservabilityEngine.cpp
    src/observability/OtlpExporter.cpp
    src/validation/ValidationFactory.cpp
    src/validation/ValidationEngine.cpp
    src/validation/internal/ContainerStateValidation.cpp
    src/validation/internal/LogValidation.cpp
    src/validation/internal/HttpValidation.cpp
    src/signals/SignalHandlerGuard.cpp
    src/core/ChaosRunner.cpp
    src/core/ObservationLoop.cpp
    src/core/RunOrchestrator.cpp
    src/interfaces/web/JsonSerializer.cpp
)

add_library(orchestrator_core STATIC ${ORCHESTRATOR_SOURCES})

add_library(Chaos::Core ALIAS orchestrator_core)

target_include_directories(orchestrator_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(orchestrator_core PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
    fmt::fmt
    LibArchive::LibArchive
    chaos_compiler_flags
    chaos_quality_flags
)

if(CHAOS_CLANG_TIDY)
    set_target_properties(orchestrator_core PROPERTIES CXX_CLANG_TIDY "${CHAOS_CLANG_TIDY}")
endif()

add_executable(orchestrator_app src/main.cpp)
target_include_directories(orchestrator_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(orchestrator_app PRIVATE Chaos::Core chaos_compiler_flags)
set_target_properties(orchestrator_app PROPERTIES OUTPUT_NAME chaos)

add_executable(orchestrator_unit_tests 
    tests/containers/ContainerEngineFactoryUnitTest.cpp
    tests/containers/internal/CgroupMetricsInternalUnitTest.cpp
    tests/containers/internal/DockerClientInternalUnitTest.cpp
    tests/containers/internal/DockerClientUnitTest.cpp
    tests/interfaces/cli/CliParserUnitTest.cpp
    tests/manifests/ManifestParserUnitTest.cpp
    tests/observability/ObservabilityEngineUnitTest.cpp
    tests/observability/OtlpExporterUnitTest.cpp
    tests/observability/ValidationEngineUnitTest.cpp
    tests/perturbations/PerturbationEngineUnitTest.cpp
    tests/perturbations/MemoryCapPerturbationUnitTest.cpp
    tests/perturbations/NetworkDelayPerturbationUnitTest.cpp
    tests/perturbations/TrafficCorruptionPerturbationUnitTest.cpp
    tests/perturbations/PerturbationTests.cpp
    tests/perturbations/internal/PacketFloodInternalUnitTest.cpp
    tests/shared/StateBroadcasterUnitTest.cpp
    tests/signals/SignalHandlerGuardUnitTest.cpp
    tests/validation/internal/HttpValidatorBaseUnitTest.cpp
    tests/core/ChaosRunnerUnitTest.cpp
    tests/core/SharedStateUnitTest.cpp
    tests/core/ObservationLoopInternalUnitTest.cpp
    tests/core/ObservationLoopUnitTest.cpp
    tests/core/RunOrchestratorUnitTest.cpp
    tests/web/JsonSerializerUnitTest.cpp
)

target_include_directories(orchestrator_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/perturbations
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(orchestrator_unit_tests PRIVATE
    Chaos::Core
    GTest::gmock
    GTest::gtest_main
)

add_executable(orchestrator_smoke_tests 
    tests/smoke/DockerClientIntegrationTest.cpp
    tests/smoke/ContainerLifecycleTest.cpp
    tests/smoke/ContainerExecTest.cpp
    tests/smoke/ContainerResourceTest.cpp
    tests/smoke/ContainerNetworkTest.cpp
    tests/smoke/ContainerValidationTest.cpp
)

target_include_directories(orchestrator_smoke_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke
)
target_compile_definitions(orchestrator_smoke_tests PRIVATE
    CHAOS_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)
target_link_libraries(orchestrator_smoke_tests PRIVATE
    Chaos::Core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(orchestrator_unit_tests PROPERTIES LABELS "unit")
gtest_discover_tests(orchestrator_smoke_tests PROPERTIES LABELS "smoke")

if(CHAOS_ENABLE_COVERAGE)
    add_custom_target(coverage_report
        COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/%m.profraw
            ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
        COMMAND llvm-profdata-18 merge -sparse ${CMAKE_BINARY_DIR}/*.profraw
            -o ${CMAKE_BINARY_DIR}/default.profdata
        COMMAND llvm-cov-18 export $<TARGET_FILE:orchestrator_unit_tests>
            -instr-profile=${CMAKE_BINARY_DIR}/default.profdata
            -format=lcov
            -ignore-filename-regex="tests/|vcpkg/"
            > ${CMAKE_BINARY_DIR}/coverage.lcov
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating automated coverage report in LCOV format..."
    )
endif()