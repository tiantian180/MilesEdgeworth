cmake_minimum_required(VERSION 3.21)

project(MilesEdgeworthDesktop VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets Multimedia Network QuickControls2)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    qt_policy(SET QTP0004 NEW)
endif()

set(MILES_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(MILES_BUILTIN_SKIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/resources/skins/miles-edgeworth")
set(MILES_GENERATED_CLIPS_QRC "${MILES_BUILTIN_SKIN_ROOT}/generated/clips.qrc")
set(MILES_SPLIT_MANIFEST_CLIPS_SCRIPT "${MILES_REPOSITORY_ROOT}/tools/split_manifest_clips.py")

add_custom_target(GenerateMilesClips
    COMMAND ${Python3_EXECUTABLE}
            "${MILES_SPLIT_MANIFEST_CLIPS_SCRIPT}"
            "${MILES_BUILTIN_SKIN_ROOT}"
    BYPRODUCTS "${MILES_GENERATED_CLIPS_QRC}"
    WORKING_DIRECTORY "${MILES_REPOSITORY_ROOT}"
    COMMENT "Generating Miles pre-cut animation clips"
    VERBATIM
)

set_source_files_properties("${MILES_GENERATED_CLIPS_QRC}" PROPERTIES GENERATED TRUE)

function(add_miles_generated_clips_qrc target_name)
    target_sources(${target_name} PRIVATE "${MILES_GENERATED_CLIPS_QRC}")
    add_dependencies(${target_name} GenerateMilesClips)
    set_property(TARGET ${target_name} APPEND PROPERTY AUTOGEN_TARGET_DEPENDS GenerateMilesClips)
endfunction()

set(DESKTOP_SOURCES
    src/DesktopShellController.cpp
    src/DesktopShellController.h
    src/chat/ChatController.cpp
    src/chat/ChatController.h
    src/chat/ChatStreamEvent.cpp
    src/chat/ChatStreamEvent.h
    src/chat/ChatTextPacer.cpp
    src/chat/ChatTextPacer.h
    src/logging/MilesLogHandler.cpp
    src/logging/MilesLogHandler.h
    src/main.cpp
    src/settings/ProviderConfigFile.cpp
    src/settings/ProviderConfigFile.h
    src/settings/SettingsController.cpp
    src/settings/SettingsController.h
    src/settings/SettingsLogging.cpp
    src/settings/SettingsLogging.h
    src/settings/SettingsService.cpp
    src/settings/SettingsService.h
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
    src/pet/manifest/PersonaStore.cpp
    src/pet/manifest/PersonaStore.h
    src/pet/manifest/SkinManifest.h
    src/pet/manifest/SkinManifestLoader.cpp
    src/pet/manifest/SkinManifestLoader.h
    src/pet/PetLogging.cpp
    src/pet/PetLogging.h
    src/pet/PetRuntime.cpp
    src/pet/PetRuntime.h
    src/pet/PetRuntimeSkin.cpp
    src/pet/requests/ActionRequest.h
    src/pet/runtime/RuntimeSnapshot.h
    src/pet/selection/AnimationPoolSelector.cpp
    src/pet/selection/AnimationPoolSelector.h
    src/pet/selection/ExpressionMappingResolver.cpp
    src/pet/selection/ExpressionMappingResolver.h
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
        qml/ChatBubbleWindow.qml
        qml/ChatWindow.qml
        qml/PetWindow.qml
        qml/SettingsWindow.qml
    SOURCES
        src/DesktopShellController.h
        src/chat/ChatController.h
        src/pet/events/PetEventBridge.h
        src/pet/PetRuntime.h
        src/settings/SettingsController.h
)

target_include_directories(MilesEdgeworthDesktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/chat
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
        ${CMAKE_CURRENT_SOURCE_DIR}/src/settings
)

target_link_libraries(MilesEdgeworthDesktop
    PRIVATE
        Qt6::Quick
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
        Qt6::QuickControls2
)

add_miles_generated_clips_qrc(MilesEdgeworthDesktop)

set_target_properties(MilesEdgeworthDesktop PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "MilesEdgeworth v2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.tian.MilesEdgeworth.v2"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(MilesEdgeworthDesktop)

if(APPLE)
    add_custom_command(TARGET MilesEdgeworthDesktop POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign -
                "$<TARGET_BUNDLE_DIR:MilesEdgeworthDesktop>"
        COMMENT "Ad-hoc signing MilesEdgeworthDesktop.app for local macOS runs"
        VERBATIM
    )
endif()

if(TARGET MilesAgentCore)
    add_custom_target(CopyMilesAgentSidecar
        ALL
        DEPENDS MilesAgentCore
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:MilesEdgeworthDesktop>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MILES_AGENT_BINARY}"
                "$<TARGET_FILE_DIR:MilesEdgeworthDesktop>/miles-agent${CMAKE_EXECUTABLE_SUFFIX}"
        VERBATIM
    )
    add_dependencies(MilesEdgeworthDesktop CopyMilesAgentSidecar)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(ChatStreamEventParserSmoke
        tests/chat_stream_event_parser_smoke.cpp
        src/chat/ChatStreamEvent.cpp
        src/chat/ChatStreamEvent.h
    )

    target_include_directories(ChatStreamEventParserSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ChatStreamEventParserSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME chat_stream_event_parser_smoke COMMAND ChatStreamEventParserSmoke)

    add_executable(ChatTextPacerSmoke
        tests/chat_text_pacer_smoke.cpp
        src/chat/ChatTextPacer.cpp
        src/chat/ChatTextPacer.h
    )

    target_include_directories(ChatTextPacerSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ChatTextPacerSmoke
        PRIVATE
            Qt6::Core
            Qt6::Test
    )

    add_test(NAME chat_text_pacer_smoke COMMAND ChatTextPacerSmoke)

    add_executable(LoggingFormatterSmoke
        tests/logging_formatter_smoke.cpp
        src/logging/MilesLogHandler.cpp
        src/logging/MilesLogHandler.h
    )

    target_include_directories(LoggingFormatterSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LoggingFormatterSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME logging_formatter_smoke COMMAND LoggingFormatterSmoke)

    add_executable(SkinManifestLoaderSmoke
        tests/skin_manifest_loader_smoke.cpp
        src/pet/manifest/PersonaStore.cpp
        src/pet/manifest/PersonaStore.h
        src/pet/manifest/SkinManifestLoader.cpp
        resources/pet_assets.qrc
    )

    target_link_libraries(SkinManifestLoaderSmoke
        PRIVATE
            Qt6::Core
    )

    add_miles_generated_clips_qrc(SkinManifestLoaderSmoke)

    target_include_directories(SkinManifestLoaderSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME skin_manifest_loader_smoke COMMAND SkinManifestLoaderSmoke)

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
        src/pet/manifest/PersonaStore.cpp
        src/pet/manifest/PersonaStore.h
        src/pet/manifest/SkinManifest.h
        src/pet/manifest/SkinManifestLoader.cpp
        src/pet/manifest/SkinManifestLoader.h
        src/pet/PetLogging.cpp
        src/pet/PetLogging.h
        src/pet/PetRuntime.cpp
        src/pet/PetRuntime.h
        src/pet/PetRuntimeSkin.cpp
        src/pet/requests/ActionRequest.h
        src/pet/runtime/RuntimeSnapshot.h
        src/pet/selection/AnimationPoolSelector.cpp
        src/pet/selection/AnimationPoolSelector.h
        src/pet/selection/ExpressionMappingResolver.cpp
        src/pet/selection/ExpressionMappingResolver.h
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

    add_miles_generated_clips_qrc(PetRuntimeSmoke)

    add_test(
        NAME pet_runtime_smoke
        COMMAND PetRuntimeSmoke
    )

    qt_add_executable(ChatControllerSmoke
        tests/chat_controller_smoke.cpp
        src/chat/ChatController.cpp
        src/chat/ChatController.h
        src/chat/ChatStreamEvent.cpp
        src/chat/ChatStreamEvent.h
        src/chat/ChatTextPacer.cpp
        src/chat/ChatTextPacer.h
        src/settings/ProviderConfigFile.cpp
        src/settings/ProviderConfigFile.h
        src/settings/SettingsLogging.cpp
        src/settings/SettingsLogging.h
        src/settings/SettingsService.cpp
        src/settings/SettingsService.h
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
        src/pet/manifest/PersonaStore.cpp
        src/pet/manifest/PersonaStore.h
        src/pet/manifest/SkinManifest.h
        src/pet/manifest/SkinManifestLoader.cpp
        src/pet/manifest/SkinManifestLoader.h
        src/pet/PetLogging.cpp
        src/pet/PetLogging.h
        src/pet/PetRuntime.cpp
        src/pet/PetRuntime.h
        src/pet/PetRuntimeSkin.cpp
        src/pet/requests/ActionRequest.h
        src/pet/runtime/RuntimeSnapshot.h
        src/pet/selection/AnimationPoolSelector.cpp
        src/pet/selection/AnimationPoolSelector.h
        src/pet/selection/ExpressionMappingResolver.cpp
        src/pet/selection/ExpressionMappingResolver.h
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.h
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h
        resources/pet_assets.qrc
    )

    target_include_directories(ChatControllerSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
    )

    target_link_libraries(ChatControllerSmoke
        PRIVATE
            Qt6::Core
            Qt6::Qml
            Qt6::Network
    )

    add_miles_generated_clips_qrc(ChatControllerSmoke)

    add_test(NAME chat_controller_smoke COMMAND ChatControllerSmoke)

    qt_add_executable(SettingsServiceSmoke
        tests/settings_service_smoke.cpp
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
        src/pet/manifest/PersonaStore.cpp
        src/pet/manifest/PersonaStore.h
        src/pet/manifest/SkinManifest.h
        src/pet/manifest/SkinManifestLoader.cpp
        src/pet/manifest/SkinManifestLoader.h
        src/pet/PetLogging.cpp
        src/pet/PetLogging.h
        src/pet/PetRuntime.cpp
        src/pet/PetRuntime.h
        src/pet/PetRuntimeSkin.cpp
        src/pet/requests/ActionRequest.h
        src/pet/runtime/RuntimeSnapshot.h
        src/pet/selection/AnimationPoolSelector.cpp
        src/pet/selection/AnimationPoolSelector.h
        src/pet/selection/ExpressionMappingResolver.cpp
        src/pet/selection/ExpressionMappingResolver.h
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.cpp
        src/skins/miles-edgeworth/MilesEdgeworthInteractions.h
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.cpp
        src/skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h
        src/settings/SettingsController.cpp
        src/settings/SettingsController.h
        src/settings/SettingsLogging.cpp
        src/settings/SettingsLogging.h
        src/settings/SettingsService.cpp
        src/settings/SettingsService.h
        src/settings/ProviderConfigFile.cpp
        src/settings/ProviderConfigFile.h
        resources/pet_assets.qrc
    )

    target_include_directories(SettingsServiceSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pet/events
    )

    target_link_libraries(SettingsServiceSmoke
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
    )

    add_miles_generated_clips_qrc(SettingsServiceSmoke)

    add_test(NAME settings_service_smoke COMMAND SettingsServiceSmoke)
endif()
