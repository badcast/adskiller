cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${INCL_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.qrc)

add_library(pixelblast SHARED ${SRC_FILES})

target_include_directories(pixelblast PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(pixelblast PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)
target_compile_definitions(pixelblast PRIVATE PB_SHARED)
target_link_libraries(pixelblast PRIVATE Qt6::Core Qt6::Widgets Qt6::Multimedia)
