cmake_minimum_required(VERSION 3.16)

project(PictureTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.3 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(PictureTool WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ImageCanvas.h
    src/ImageCanvas.cpp
    src/Document.h
    src/Document.cpp
    src/ImageOps.h
    src/ImageOps.cpp
    src/Painters.h
    src/Painters.cpp
    src/LayerPanel.h
    src/LayerPanel.cpp
    src/ToolBox.h
    src/ToolBox.cpp
    src/ToolOptions.h
    src/ToolOptions.cpp
    src/AiServer.h
    src/AiServer.cpp
    src/Commands.h
    src/dialogs.h
    src/dialogs.cpp
    src/IconFactory.h
    src/IconFactory.cpp
    src/Theme.h
    src/Theme.cpp
    src/TitleBar.h
    src/TitleBar.cpp
    resources/resources.qrc
    resources/app.rc
)

target_link_libraries(PictureTool PRIVATE
    Qt6::Widgets
    Qt6::Network
)

# MSVC 下启用 UTF-8 源码支持
if(MSVC)
    target_compile_options(PictureTool PRIVATE /utf-8)
endif()
