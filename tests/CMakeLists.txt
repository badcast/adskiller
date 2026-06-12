cmake_minimum_required(VERSION 3.20)

find_package(Qt6 REQUIRED COMPONENTS Core Test Network Widgets)

add_executable(ServiceTests
    TestMyDeviceService.cpp
)

target_include_directories(ServiceTests PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/src/services
    ${CMAKE_SOURCE_DIR}/src/common/adbfront/include
    ${CMAKE_SOURCE_DIR}/src/widgets/Include
)

target_link_libraries(ServiceTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
    core
    services
    adbfront
    widgets
)

add_test(NAME ServiceTests COMMAND ServiceTests)
