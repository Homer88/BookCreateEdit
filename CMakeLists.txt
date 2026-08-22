cmake_minimum_required(VERSION 3.16)
project(BookCreateEdit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Xml)

set(CORE_SOURCES
    source/BookDocument.cpp
    source/HtmlConverter.cpp
)

set(CORE_HEADERS
    source/BookDocument.h
    source/HtmlConverter.h
    source/interfaces/IBookPlugin.h
    source/interfaces/IBookFormat.h
)

add_library(BookCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(BookCore PUBLIC
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/interfaces
)
target_link_libraries(BookCore PUBLIC Qt5::Widgets Qt5::Xml)

set(SOURCES
    source/main.cpp
    source/MainWindow.cpp
    source/RibbonWidget.cpp
    source/LeftPanel.cpp
    source/SettingsDialog.cpp
    source/SettingsManager.cpp
    source/PublishDialog.cpp
    source/RestoreDialog.cpp
    source/XmlEditor.cpp
    source/EditorTabs.cpp
    source/PluginManager.cpp
    source/ImportDialog.cpp
    source/ThemeManager.cpp
)

set(HEADERS
    source/MainWindow.h
    source/RibbonWidget.h
    source/LeftPanel.h
    source/SettingsDialog.h
    source/SettingsManager.h
    source/PublishDialog.h
    source/RestoreDialog.h
    source/XmlEditor.h
    source/EditorTabs.h
    source/PluginManager.h
    source/ImportDialog.h
    source/ThemeManager.h
)

set(RESOURCES
    source/resources.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE BookCore)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/source/interfaces
)

add_subdirectory(${CMAKE_SOURCE_DIR}/source/format formats)
add_subdirectory(${CMAKE_SOURCE_DIR}/source/plugin plugins)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/source/icons"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icons"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/source/lang"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/lang"
    COMMENT "Copying resource directories..."
)
