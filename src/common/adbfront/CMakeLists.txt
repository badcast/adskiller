cmake_minimum_required(VERSION 3.16)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${INCL_DIR}/*.h)
add_library(adbfront OBJECT ${SRC_FILES})
target_link_libraries(adbfront PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_include_directories(adbfront PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)

