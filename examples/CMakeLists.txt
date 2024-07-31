cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)

add_executable(FiniteStateMachineExample "FiniteStateMachine.cpp")

target_link_libraries(FiniteStateMachineExample PRIVATE FiniteStateMachine)

