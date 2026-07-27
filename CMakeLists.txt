cmake_minimum_required(VERSION 3.28)

project(chaos
    VERSION 0.4.4 
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

option(CHAOS_ENABLE_ASAN_UBSAN "Enable Address and Undefined Behavior Sanitizers" OFF)
option(CHAOS_ENABLE_TSAN "Enable Thread Sanitizer (Incompatible with ASan)" OFF)
option(CHAOS_ENABLE_COVERAGE "Enable LLVM Source-Based Code Coverage" OFF)

add_library(chaos_quality_flags INTERFACE)

if(CHAOS_ENABLE_ASAN_UBSAN AND CHAOS_ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled at the same time. Choose one.")
endif()

if(CHAOS_ENABLE_ASAN_UBSAN)
    message(STATUS "Enabling Clang Memory Sanitizers (ASan + UBSan)")
    target_compile_options(chaos_quality_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(chaos_quality_flags INTERFACE -fsanitize=address,undefined)
elseif(CHAOS_ENABLE_TSAN)
    message(STATUS "Enabling Clang Thread Sanitizer (TSan)")
    target_compile_options(chaos_quality_flags INTERFACE -fsanitize=thread)
    target_link_options(chaos_quality_flags INTERFACE -fsanitize=thread)
    # cpp-httplib's async getaddrinfo (getaddrinfo_a) spawns glibc internal
    # threads that crash under TSan's malloc interceptor. Fall back to sync DNS.
    target_compile_options(chaos_quality_flags INTERFACE -UCPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
endif()

if(CHAOS_ENABLE_COVERAGE)
    message(STATUS "Enabling LLVM Code Coverage")
    target_compile_options(chaos_quality_flags INTERFACE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(chaos_quality_flags INTERFACE -fprofile-instr-generate -fcoverage-mapping)
endif()

add_subdirectory(orchestrator)
add_subdirectory(tests/e2e)

if(EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()
