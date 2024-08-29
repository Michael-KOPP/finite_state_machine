cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)

add_library(finite_state_machine INTERFACE "fsm/finite_state_machine.hpp")

target_include_directories(finite_state_machine INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY fsm DESTINATION include)

install(TARGETS finite_state_machine
        EXPORT finite_state_machineTargets
        INCLUDES DESTINATION include/fsm)

install(EXPORT finite_state_machineTargets
        FILE fsmConfig.cmake
        NAMESPACE fsm::
        DESTINATION lib/cmake/fsm)
