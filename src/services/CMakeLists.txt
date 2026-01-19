cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${INCL_DIR}/*.h)

add_library(services STATIC ${SRC_FILES})
target_include_directories(services PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(services PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)

add_subdirectory(3rdParty/VCard)
add_subdirectory(3rdParty/ClassfieldPhoneNumber)

target_link_libraries(services PRIVATE VCard classfieldnum Qt6::Core Qt6::Widgets Qt6::Network)
