cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)

add_library(FiniteStateMachine INTERFACE "FiniteStateMachine.h")

target_include_directories(FiniteStateMachine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
