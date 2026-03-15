add_executable(chaos-wrapper src/main.cpp)

target_link_options(chaos-wrapper PRIVATE "-static")

if(UNIX)
    target_compile_options(chaos-wrapper PRIVATE
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
    )
endif()
