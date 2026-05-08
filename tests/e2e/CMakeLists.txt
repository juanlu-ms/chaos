find_package(GTest CONFIG REQUIRED)

add_executable(chaos_e2e_tests
    PerturbationE2eTest.cpp
    ObserveValidateE2eTest.cpp
    ManifestE2eTest.cpp
)

target_link_libraries(chaos_e2e_tests PRIVATE
    orchestrator_core
    GTest::gtest_main
)

target_compile_definitions(chaos_e2e_tests PRIVATE
    CHAOS_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/examples"
)

include(GoogleTest)
gtest_discover_tests(chaos_e2e_tests)
