cmake_minimum_required(VERSION 3.21)

project(MilesEdgeworthDesktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets Multimedia)

qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    qt_policy(SET QTP0004 NEW)
endif()

set(DESKTOP_SOURCES
    src/DesktopShellController.cpp
    src/DesktopShellController.h
    src/main.cpp
    src/window/WindowInputMaskController.cpp
    src/window/WindowInputMaskController.h
    src/pet/behavior/BehaviorTriggerEngine.cpp
    src/pet/behavior/BehaviorTriggerEngine.h
    src/pet/commands/SkinCommand.h
    src/pet/commands/SkinCommandResolver.cpp
    src/pet/commands/SkinCommandResolver.h
    src/pet/effects/AudioController.cpp
    src/pet/effects/AudioController.h
    src/pet/effects/PropController.cpp
    src/pet/effects/PropController.h
    src/pet/effects/PropState.h
    src/pet/events/PetEvent.h
    src/pet/events/PetEventBridge.cpp
    src/pet/events/PetEventBridge.h
    src/pet/interaction/CustomInteractionRegistry.cpp
    src/pet/interaction/CustomInteractionRegistry.h
    src/pet/interaction/GestureTracker.cpp
    src/pet/interaction/GestureTracker.h
    src/pet/interaction/HitZoneMatcher.cpp
    src/pet/interaction/HitZoneMatcher.h
    src/pet/interaction/InteractionPipeline.cpp
    src/pet/interaction/InteractionPipeline.h
    src/pet/manifest/SkinManifest.h
    src/pet/manifest/SkinManifestLoader.cpp
    src/pet/manifest/SkinManifestLoader.h
    src/pet/PetRuntime.cpp
    src/pet/PetRuntime.h
    src/pet/requests/ActionRequest.h
    src/pet/runtime/RuntimeSnapshot.h
    src/pet/selection/ActionPoolSelector.cpp
    src/pet/selection/ActionPoolSelector.h
    src/pet/surface/PetContextMenu.cpp
    src/pet/surface/PetContextMenu.h
    src/pet/surface/PetSurfaceWindow.cpp
    src/pet/surface/PetSurfaceWindow.h
    src/pet/surface/PropSurfaceWindow.cpp
    src/pet/surface/PropSurfaceWindow.h
    src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp
    src/skins/miles-edgeworth/MilesEdgeworthInteractions.h
    src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp
    src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h
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
    SOURCES
        src/DesktopShellController.h
        src/pet/events/PetEventBridge.h
        src/pet/PetRuntime.h
)

target_include_directories(MilesEdgeworthDesktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
)

target_link_libraries(MilesEdgeworthDesktop
    PRIVATE
        Qt6::Quick
        Qt6::Widgets
        Qt6::Multimedia
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

if(BUILD_TESTING)
    qt_add_executable(PetRuntimeSmoke
        tests/pet_runtime_smoke.cpp
        src/pet/behavior/BehaviorTriggerEngine.cpp
        src/pet/behavior/BehaviorTriggerEngine.h
        src/pet/commands/SkinCommand.h
        src/pet/commands/SkinCommandResolver.cpp
        src/pet/commands/SkinCommandResolver.h
        src/pet/effects/AudioController.cpp
        src/pet/effects/AudioController.h
        src/pet/effects/PropController.cpp
        src/pet/effects/PropController.h
        src/pet/effects/PropState.h
        src/pet/events/PetEvent.h
        src/pet/events/PetEventBridge.cpp
        src/pet/events/PetEventBridge.h
        src/pet/interaction/CustomInteractionRegistry.cpp
        src/pet/interaction/CustomInteractionRegistry.h
        src/pet/interaction/GestureTracker.cpp
        src/pet/interaction/GestureTracker.h
        src/pet/interaction/HitZoneMatcher.cpp
        src/pet/interaction/HitZoneMatcher.h
        src/pet/interaction/InteractionPipeline.cpp
        src/pet/interaction/InteractionPipeline.h
        src/pet/manifest/SkinManifest.h
        src/pet/manifest/SkinManifestLoader.cpp
        src/pet/manifest/SkinManifestLoader.h
        src/pet/PetRuntime.cpp
        src/pet/PetRuntime.h
        src/pet/requests/ActionRequest.h
        src/pet/runtime/RuntimeSnapshot.h
        src/pet/selection/ActionPoolSelector.cpp
        src/pet/selection/ActionPoolSelector.h
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.h
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h
        resources/pet_assets.qrc
    )

    target_include_directories(PetRuntimeSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
    )

    target_link_libraries(PetRuntimeSmoke
        PRIVATE
            Qt6::Core
            Qt6::Qml
    )

    add_test(
        NAME pet_runtime_smoke
        COMMAND PetRuntimeSmoke
    )
endif()
