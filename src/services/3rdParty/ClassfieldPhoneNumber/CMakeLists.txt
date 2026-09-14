# Source From https://github.com/badcast/ClassfieldPhoneNumber

cmake_minimum_required(VERSION 3.20)

set(INCL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(classfieldnum STATIC include/NumberPreview.h include/Classfieldraw.h src/Classfieldnum.cpp)
target_include_directories(classfieldnum PUBLIC ${INCL_DIR})
target_include_directories(classfieldnum INTERFACE $<BUILD_INTERFACE:${INCL_DIR}>
                                           $<INSTALL_INTERFACE:${INCL_DIR}>)
