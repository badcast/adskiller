cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB SOURCE_QRC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.qrc")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${INCL_DIR}/*.h")

set(CALLBACK_URL "")
if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_SOURCE_DIR}/.env" ENV_LINES ENCODING UTF-8)
    foreach(ENV_LINE IN LISTS ENV_LINES)
        string(STRIP "${ENV_LINE}" ENV_LINE)
        if(ENV_LINE MATCHES "^(export[ \t]+)?CALLBACK_URL[ \t]*=[ \t]*(.*)$")
            set(CALLBACK_URL "${CMAKE_MATCH_2}")
            break()
        endif()
    endforeach()
endif()
string(STRIP "${CALLBACK_URL}" CALLBACK_URL)

qt_add_resources(APP_RESOURCES
    ${SOURCE_QRC}
)

add_library(pixelblast SHARED ${SOURCE_FILES} ${APP_RESOURCES})
if(WIN32)
    set_target_properties(pixelblast PROPERTIES PREFIX "")
endif()
if(CALLBACK_URL)
    target_compile_definitions(pixelblast PRIVATE CALLBACK_URL="${CALLBACK_URL}")
endif()
target_include_directories(pixelblast PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/ui)
target_include_directories(pixelblast PUBLIC $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:include>)
target_compile_definitions(pixelblast PRIVATE PB_SHARED)
target_link_libraries(pixelblast PRIVATE Qt6::Core Qt6::Widgets Qt6::Multimedia Qt6::Network)
