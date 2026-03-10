project(chaos_shared_protocol)

add_library(shared_protocol INTERFACE)

target_include_directories(shared_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
