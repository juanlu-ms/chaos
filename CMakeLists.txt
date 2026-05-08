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

find_program(CLANG_TIDY_BIN clang-tidy)
if(CLANG_TIDY_BIN)
  message(STATUS "clang-tidy found: ${CLANG_TIDY_BIN}")
  set(CHAOS_CLANG_TIDY "${CLANG_TIDY_BIN}" CACHE INTERNAL "clang-tidy binary path")
else()
  message(STATUS "clang-tidy not found — skipping static analysis")
endif()

enable_testing()

file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/orchestrator/src/*.cpp
    ${CMAKE_SOURCE_DIR}/orchestrator/src/*.hpp
    ${CMAKE_SOURCE_DIR}/orchestrator/include/*.hpp
    ${CMAKE_SOURCE_DIR}/orchestrator/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/wrapper/src/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/e2e/*.cpp
    ${CMAKE_SOURCE_DIR}/examples/*.cpp
)

find_program(CLANG_FORMAT_BIN clang-format)
if(CLANG_FORMAT_BIN)
  message(STATUS "clang-format found: ${CLANG_FORMAT_BIN}")
  add_custom_target(check-format
      COMMAND ${CLANG_FORMAT_BIN} --dry-run -Werror ${ALL_SOURCE_FILES}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Checking code format with clang-format..."
  )
  add_custom_target(format
      COMMAND ${CLANG_FORMAT_BIN} -i ${ALL_SOURCE_FILES}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Formatting code with clang-format..."
  )
else()
  message(STATUS "clang-format not found — skipping format targets")
  add_custom_target(check-format
      COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — install it or add to PATH"
      COMMENT "check-format unavailable (clang-format not found)"
  )
  add_custom_target(format
      COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — install it or add to PATH"
      COMMENT "format unavailable (clang-format not found)"
  )
endif()

add_subdirectory(wrapper)
add_subdirectory(orchestrator)
add_subdirectory(tests/e2e)
