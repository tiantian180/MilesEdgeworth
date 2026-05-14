cmake_minimum_required(VERSION 3.21)

project(MilesEdgeworth VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    MilesEdgeworth.cpp
    MilesEdgeworth.h
    MilesEdgeworth.ui
    MyGraphicsView.cpp
    MyGraphicsView.h
    PicViewer.cpp
    PicViewer.h
    PicViewer.ui
    ProsecutorBadge.cpp
    ProsecutorBadge.h
    ProsecutorBadge.ui
    MilesEdgeworth.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES MilesEdgeworth.rc)
endif()

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND PROJECT_SOURCES
        MacWindowLevel.h
        MacWindowLevel.mm
    )
endif()

qt_add_executable(MilesEdgeworth
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_include_directories(MilesEdgeworth
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MilesEdgeworth
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
)

set_target_properties(MilesEdgeworth PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "MilesEdgeworth"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tian.MilesEdgeworth"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(MilesEdgeworth)
