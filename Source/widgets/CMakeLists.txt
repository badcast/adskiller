set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Include)

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS "*.hpp" "*.h")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "*.cpp" "*.c")
add_library(widgets STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(widgets PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(widgets PUBLIC ${INCL_DIR} $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)
target_link_libraries(widgets PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
