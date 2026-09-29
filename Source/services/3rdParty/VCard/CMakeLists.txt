# Source From https://github.com/ivanenko/vCard

cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE HEADER_FILES "*.hpp" "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.c")
add_library(VCard STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(VCard PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(VCard PROPERTIES BUILD_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
