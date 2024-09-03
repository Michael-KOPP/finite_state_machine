cmake_minimum_required (VERSION 3.8)

project ("finite_state_machine")

add_subdirectory(src)

option(EXAMPLES OFF)

if(EXAMPLES)
    add_subdirectory(examples)
endif()
