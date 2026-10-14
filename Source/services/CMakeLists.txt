cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB SRC_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(services STATIC ${SRC_FILES})
target_include_directories(services PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/Source/UI)
target_include_directories(services PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/VCard ${CMAKE_CURRENT_BINARY_DIR}/vcard)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/ClassfieldPhoneNumber ${CMAKE_CURRENT_BINARY_DIR}/classfieldnum)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/PixelBlastGame ${CMAKE_CURRENT_BINARY_DIR}/pixelblast)

target_link_libraries(services PUBLIC VCard classfieldnum pixelblast PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
