cmake_minimum_required(VERSION 3.16)
project(Plugins VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Xml)

add_subdirectory(spellchecker)
add_subdirectory(diagramrenderer)
