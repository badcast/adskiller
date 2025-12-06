set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Include)
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${INCL_DIR}/*.h)

add_library(widgets STATIC ${SRC_FILES})
target_include_directories(widgets PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(widgets PUBLIC ${INCL_DIR} $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)
target_link_libraries(widgets PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
