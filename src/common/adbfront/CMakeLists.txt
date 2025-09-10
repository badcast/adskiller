cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${INCL_DIR}/*.h)
add_library(adbfront STATIC ${SRC_FILES})
target_link_libraries(adbfront PRIVATE Qt6::Core)
target_include_directories(adbfront PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)

