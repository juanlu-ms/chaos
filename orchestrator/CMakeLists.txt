find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

# ---------------------------------------------------------------------------
# Web UI embedding (CMakeRC + automated pnpm build)
# ---------------------------------------------------------------------------
option(CHAOS_EMBED_WEB_UI "Build and embed the Web UI assets into the binary" ON)

if(CHAOS_EMBED_WEB_UI)
    find_program(CHAOS_PNPM_BIN pnpm)
    if(NOT CHAOS_PNPM_BIN)
        message(STATUS "pnpm not found — skipping Web UI embedding (install pnpm to enable)")
        set(CHAOS_EMBED_WEB_UI OFF CACHE BOOL "Build and embed the Web UI assets into the binary" FORCE)
    endif()
endif()

if(CHAOS_EMBED_WEB_UI)
    set(chaos_web_root "${CMAKE_BINARY_DIR}/generated/web")
    set(frontend_dir "${CMAKE_SOURCE_DIR}/frontend")
    set(frontend_stamp "${CMAKE_BINARY_DIR}/chaos_frontend.stamp")

    # Track frontend sources for staleness detection.
    file(GLOB_RECURSE CHAOS_FRONTEND_SOURCES
        "${frontend_dir}/src/*"
        "${frontend_dir}/index.html"
        "${frontend_dir}/package.json"
        "${frontend_dir}/pnpm-lock.yaml"
        "${frontend_dir}/vite.config.js")

    # Standard stamp-file staleness check: rebuild only when sources change.
    set(chaos_need_frontend_build FALSE)
    if(NOT EXISTS "${frontend_stamp}")
        set(chaos_need_frontend_build TRUE)
    elseif(NOT EXISTS "${chaos_web_root}/index.html")
        set(chaos_need_frontend_build TRUE)
    else()
        file(TIMESTAMP "${frontend_stamp}" chaos_stamp_ts "%s" UTC)
        foreach(src IN LISTS CHAOS_FRONTEND_SOURCES)
            file(TIMESTAMP "${src}" chaos_src_ts "%s" UTC)
            if(chaos_src_ts GREATER chaos_stamp_ts)
                set(chaos_need_frontend_build TRUE)
                break()
            endif()
        endforeach()
    endif()

    if(chaos_need_frontend_build)
        message(STATUS "Building frontend (pnpm install + vite build)...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env "CHAOS_WEB_OUT_DIR=${chaos_web_root}"
                    ${CHAOS_PNPM_BIN} install
            WORKING_DIRECTORY "${frontend_dir}"
            RESULT_VARIABLE chaos_pnpm_result
            OUTPUT_VARIABLE chaos_pnpm_output
            ERROR_VARIABLE chaos_pnpm_output)
        if(NOT chaos_pnpm_result EQUAL 0)
            message(FATAL_ERROR "pnpm install failed (code ${chaos_pnpm_result}):\n${chaos_pnpm_output}")
        endif()
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env "CHAOS_WEB_OUT_DIR=${chaos_web_root}"
                    ${CHAOS_PNPM_BIN} run build
            WORKING_DIRECTORY "${frontend_dir}"
            RESULT_VARIABLE chaos_vite_result
            OUTPUT_VARIABLE chaos_vite_output
            ERROR_VARIABLE chaos_vite_output)
        if(NOT chaos_vite_result EQUAL 0)
            message(FATAL_ERROR "vite build failed (code ${chaos_vite_result}):\n${chaos_vite_output}")
        endif()
        file(WRITE "${frontend_stamp}" "built at ${CMAKE_CURRENT_TIMESTAMP}")
        message(STATUS "Frontend build complete.")
    else()
        message(STATUS "Frontend up-to-date (skipping build).")
    endif()

    # Manual rebuild target (re-run cmake configure afterwards to re-embed).
    add_custom_target(chaos_frontend
        COMMAND ${CMAKE_COMMAND} -E env "CHAOS_WEB_OUT_DIR=${chaos_web_root}"
                ${CHAOS_PNPM_BIN} install
        COMMAND ${CMAKE_COMMAND} -E env "CHAOS_WEB_OUT_DIR=${chaos_web_root}"
                ${CHAOS_PNPM_BIN} run build
        COMMAND ${CMAKE_COMMAND} -E touch "${frontend_stamp}"
        WORKING_DIRECTORY "${frontend_dir}"
        COMMENT "Force-rebuilding frontend (run 'cmake --preset ...' afterwards to re-embed)"
        VERBATIM)

    # Embed built assets via CMakeRC.
    file(GLOB_RECURSE CHAOS_WEB_ASSETS
         CONFIGURE_DEPENDS
         "${chaos_web_root}/*")
    if(CHAOS_WEB_ASSETS)
        include("${CMAKE_SOURCE_DIR}/cmake/CMakeRC.cmake")
        cmrc_add_resource_library(chaos_web_resources
            NAMESPACE chaos_web)
        cmrc_add_resources(chaos_web_resources
            WHENCE "${chaos_web_root}"
            PREFIX / ${CHAOS_WEB_ASSETS})
        list(LENGTH CHAOS_WEB_ASSETS CHAOS_WEB_ASSETS_N)
        message(STATUS "Embedded Web UI: ${CHAOS_WEB_ASSETS_N} files from ${chaos_web_root}")
    else()
        message(WARNING "CHAOS_EMBED_WEB_UI=ON but no assets found in ${chaos_web_root}")
    endif()
endif()

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
    src/observability/OtlpRunObserver.cpp
    src/observability/LoggerSetup.cpp
    src/validation/ValidationFactory.cpp
    src/validation/ValidationEngine.cpp
    src/validation/internal/ContainerStateValidation.cpp
    src/validation/internal/LogValidation.cpp
    src/validation/internal/HttpValidation.cpp
    src/signals/SignalHandlerGuard.cpp
    src/core/ChaosService.cpp
    src/core/CompositeRunObserver.cpp
    src/core/ObservationLoop.cpp
    src/core/ResultSerializer.cpp
    src/history/HistoryJson.cpp
    src/history/FileRunHistory.cpp
    src/history/RunRecorder.cpp
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

# Link embedded Web UI resources (if built).
if(CHAOS_EMBED_WEB_UI AND TARGET chaos_web_resources)
    target_compile_definitions(orchestrator_core PUBLIC CHAOS_HAVE_EMBEDDED_WEB_UI)
    target_link_libraries(orchestrator_core PUBLIC chaos_web_resources)
endif()

if(CHAOS_CLANG_TIDY)
    set_target_properties(orchestrator_core PROPERTIES CXX_CLANG_TIDY "${CHAOS_CLANG_TIDY}")
endif()

add_executable(orchestrator_app src/main.cpp)
target_include_directories(orchestrator_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(orchestrator_app PRIVATE Chaos::Core chaos_compiler_flags)
set_target_properties(orchestrator_app PROPERTIES OUTPUT_NAME chaos)

add_executable(orchestrator_unit_tests 
    tests/containers/ContainerEngineFactoryUnitTest.cpp
    tests/containers/internal/CgroupMetricsGathererUnitTest.cpp
    tests/containers/internal/CgroupMetricsInternalUnitTest.cpp
    tests/containers/internal/DockerClientInternalUnitTest.cpp
    tests/containers/internal/DockerClientUnitTest.cpp
    tests/core/ChaosServiceUnitTest.cpp
    tests/core/CompositeRunObserverUnitTest.cpp
    tests/core/ObservationLoopInternalUnitTest.cpp
    tests/core/ObservationLoopUnitTest.cpp
    tests/core/RunOrchestratorUnitTest.cpp
    tests/core/SharedStateUnitTest.cpp
    tests/history/HistoryJsonUnitTest.cpp
    tests/history/FileRunHistoryUnitTest.cpp
    tests/history/RunRecorderUnitTest.cpp
    tests/interfaces/cli/CliParserUnitTest.cpp
    tests/manifests/ManifestParserUnitTest.cpp
    tests/observability/ObservabilityEngineUnitTest.cpp
    tests/observability/OtlpExporterUnitTest.cpp
    tests/observability/OtlpRunObserverUnitTest.cpp
    tests/perturbations/CpuCapPerturbationUnitTest.cpp
    tests/perturbations/KillPerturbationUnitTest.cpp
    tests/perturbations/MemoryCapPerturbationUnitTest.cpp
    tests/perturbations/NetworkCutoffPerturbationUnitTest.cpp
    tests/perturbations/NetworkDelayPerturbationUnitTest.cpp
    tests/perturbations/PacketFloodPerturbationUnitTest.cpp
    tests/perturbations/PerturbationEngineUnitTest.cpp
    tests/perturbations/PerturbationTests.cpp
    tests/perturbations/TrafficCorruptionPerturbationUnitTest.cpp
    tests/perturbations/internal/PacketFloodInternalUnitTest.cpp
    tests/signals/SignalHandlerGuardUnitTest.cpp
    tests/validation/ValidationEngineUnitTest.cpp
    tests/validation/ValidationFactoryUnitTest.cpp
    tests/validation/internal/ContainerStateValidationUnitTest.cpp
    tests/validation/internal/HttpValidationUnitTest.cpp
    tests/validation/internal/HttpValidatorBaseUnitTest.cpp
    tests/validation/internal/LogValidationUnitTest.cpp
    tests/web/JsonSerializerUnitTest.cpp
    tests/web/ServerUnitTest.cpp
    tests/web/EmbeddedWebUiUnitTest.cpp
)

target_include_directories(orchestrator_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/perturbations
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/history
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
    tests/smoke/RunHistorySmokeTest.cpp
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

install(TARGETS orchestrator_app RUNTIME DESTINATION bin)

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