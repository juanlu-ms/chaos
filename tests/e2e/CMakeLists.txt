find_package(GTest CONFIG REQUIRED)

add_executable(chaos_e2e_tests
    E2eTestBase.hpp
    PerturbationE2eTest.cpp
    ObserveValidateE2eTest.cpp
    ManifestE2eTest.cpp
    RunOrchestratorE2eTest.cpp
    RunHistoryE2eTest.cpp
)

target_link_libraries(chaos_e2e_tests PRIVATE
    Chaos::Core
    GTest::gtest_main
)

target_compile_definitions(chaos_e2e_tests PRIVATE
    CHAOS_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)

target_include_directories(chaos_e2e_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/orchestrator/src
)

include(GoogleTest)
gtest_discover_tests(chaos_e2e_tests PROPERTIES LABELS "e2e")
