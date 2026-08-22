cmake_minimum_required(VERSION 3.16)
project(format_hlp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Xml Gui)

set(SOURCES
    hlpformat.cpp
    converter/rtf2html.cpp
    converter/wmf2png.cpp
    converter/mrb2png.cpp
    thirdparty/helpdeco/helpdeco.c
    thirdparty/helpdeco/helpdec1.c
    thirdparty/helpdeco/helpdeco_bridge.c
)

set(HEADERS
    hlpformat.h
    converter/rtf2html.h
    converter/wmf2png.h
    converter/mrb2png.h
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/converter
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/helpdeco
)

target_compile_definitions(${PROJECT_NAME} PRIVATE HELPDECO_LIBRARY)

target_link_libraries(${PROJECT_NAME} PRIVATE
    BookCore
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    Qt5::Xml
    gdi32
    gdiplus
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX ""
)
