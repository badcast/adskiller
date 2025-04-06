cmake_minimum_required(VERSION 3.3)
set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/incl)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(adbfront OBJECT ${SRC_FILES})
target_include_directories(adbfront PUBLIC
                                    $<BUILD_INTERFACE:${INCL_DIR}>)
set_target_properties(adbfront PROPERTIES PUBLIC_HEADER ${INCL_DIR})
