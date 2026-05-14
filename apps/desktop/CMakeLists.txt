cmake_minimum_required(VERSION 3.21)

project(MilesEdgeworthDesktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

set(DESKTOP_SOURCES
    src/main.cpp
    resources/pet_assets.qrc
)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND DESKTOP_SOURCES
        src/platform/MacPetWindowBehavior.h
        src/platform/MacPetWindowBehavior.mm
    )
endif()

qt_add_executable(MilesEdgeworthDesktop
    MANUAL_FINALIZATION
    ${DESKTOP_SOURCES}
)

qt_add_qml_module(MilesEdgeworthDesktop
    URI MilesEdgeworth
    VERSION 1.0
    QML_FILES
        qml/PetWindow.qml
)

target_include_directories(MilesEdgeworthDesktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(MilesEdgeworthDesktop
    PRIVATE
        Qt6::Quick
)

set_target_properties(MilesEdgeworthDesktop PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "MilesEdgeworth v2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tian.MilesEdgeworth.v2"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(MilesEdgeworthDesktop)
