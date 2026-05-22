#!/usr/bin/env python3
"""Check the Phase 2.2 settings + secret-store contract."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    secret_h = read("apps/desktop/src/settings/SecretStore.h")
    secret_cpp = read("apps/desktop/src/settings/SecretStore.cpp")
    mac_secret_h = read("apps/desktop/src/settings/MacSecretStore.h")
    mac_secret_mm = read("apps/desktop/src/settings/MacSecretStore.mm")
    settings_h = read("apps/desktop/src/settings/SettingsService.h")
    settings_cpp = read("apps/desktop/src/settings/SettingsService.cpp")
    controller_h = read("apps/desktop/src/settings/SettingsController.h")
    controller_cpp = read("apps/desktop/src/settings/SettingsController.cpp")
    settings_qml = read("apps/desktop/qml/SettingsWindow.qml")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    chat_h = read("apps/desktop/src/chat/ChatController.h")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    smoke = read("apps/desktop/tests/settings_service_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    index_doc = read("docs/v2/文档索引.md")
    stage_doc = read("docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md")

    # SecretStore interface
    require("class SecretStore" in secret_h, "SecretStore.h must declare class SecretStore")
    require("virtual bool available()" in secret_h, "SecretStore must expose available()")
    require("virtual QString read(" in secret_h, "SecretStore must expose read()")
    require("virtual bool write(" in secret_h, "SecretStore must expose write()")
    require("virtual bool remove(" in secret_h, "SecretStore must expose remove()")
    require("std::unique_ptr<SecretStore> create()" in secret_h, "factory create() must be declared")
    require("class NullSecretStore" in secret_h, "NullSecretStore must be declared")
    require(
        "Q_OS_MACOS" in secret_cpp or "MILES_HAS_MAC_SECRET_STORE" in secret_cpp,
        "SecretStore.cpp factory must branch on macOS",
    )

    # macOS implementation
    require("Security/Security.h" in mac_secret_mm, "MacSecretStore.mm must include Security framework")
    require("SecItemAdd" in mac_secret_mm, "MacSecretStore must use SecItemAdd")
    require("SecItemCopyMatching" in mac_secret_mm, "MacSecretStore must use SecItemCopyMatching")
    require("SecItemDelete" in mac_secret_mm, "MacSecretStore must use SecItemDelete")
    require(
        "kSecClassGenericPassword" in mac_secret_mm,
        "MacSecretStore must use generic-password keychain class",
    )
    require("class MacSecretStore" in mac_secret_h, "MacSecretStore.h must declare class MacSecretStore")

    # SettingsService
    require("class SettingsService" in settings_h, "SettingsService class missing")
    require("baseUrl" in settings_h and "setBaseUrl" in settings_h, "baseUrl getter/setter required")
    require("model" in settings_h and "setModel" in settings_h, "model getter/setter required")
    require("temperature" in settings_h and "setTemperature" in settings_h, "temperature getter/setter required")
    require("maxTokens" in settings_h and "setMaxTokens" in settings_h, "maxTokens getter/setter required")
    require("msPerChar" in settings_h and "setMsPerChar" in settings_h, "msPerChar getter/setter required")
    require("apiKey" in settings_h and "setApiKey" in settings_h, "apiKey accessor/setter required")
    require("secretStoreAvailable" in settings_h, "secretStoreAvailable query required")
    require("void save()" in settings_h or "Q_INVOKABLE void save" in settings_h, "save() method required")
    require("void saved()" in settings_h, "saved() signal required")
    require("QSettings" in settings_cpp, "SettingsService must persist through QSettings")
    require("provider/baseUrl" in settings_cpp, "QSettings key provider/baseUrl required")
    require("provider/model" in settings_cpp, "QSettings key provider/model required")
    require("provider/temperature" in settings_cpp, "QSettings key provider/temperature required")
    require("provider/maxTokens" in settings_cpp, "QSettings key provider/maxTokens required")
    require("chat/msPerChar" in settings_cpp, "QSettings key chat/msPerChar required")
    require("provider/apiKey" not in settings_cpp, "API key must never be stored as a QSettings key")

    # SettingsController + QML window
    require("class SettingsController" in controller_h, "SettingsController class missing")
    require("Q_INVOKABLE" in controller_h, "SettingsController must expose Q_INVOKABLE methods")
    require("openWindow" in controller_h, "SettingsController.openWindow() required")
    require("SettingsControllerForeign" in controller_h, "SettingsControllerForeign QML singleton boilerplate required")
    require("QJSEngine::setObjectOwnership" in controller_h, "SettingsController singleton must keep C++ ownership")
    require("ApplicationWindow" in settings_qml, "SettingsWindow.qml must be ApplicationWindow")
    require("echoMode" in settings_qml, "API key field must use echoMode for masking")
    require("设置" in chat_qml, "ChatWindow must surface a 设置 button")
    require("SettingsController" in chat_qml, "ChatWindow must reference the SettingsController singleton")
    require("TextEdit" in chat_qml, "chat message text must be selectable")
    require("selectByMouse: true" in chat_qml, "chat message text must support mouse selection")
    require("readOnly: true" in chat_qml, "chat message text selection must not make bubbles editable")
    require(
        "未连接，先点设置填写模型配置" in chat_qml,
        "disabled input placeholder must explain the disconnected state and settings entry",
    )
    require(
        "disconnectedInput" in chat_qml,
        "ChatWindow must expose a distinct disconnected input style",
    )

    # ChatController wiring
    require("SettingsService" in chat_h, "ChatController must take a SettingsService")
    require("QProcessEnvironment" in chat_cpp, "ChatController must build a QProcessEnvironment")
    require("MILES_PROVIDER_BASE_URL" in chat_cpp, "ChatController must inject MILES_PROVIDER_BASE_URL")
    require("MILES_PROVIDER_API_KEY" in chat_cpp, "ChatController must inject MILES_PROVIDER_API_KEY")
    require("MILES_PROVIDER_MODEL" in chat_cpp, "ChatController must inject MILES_PROVIDER_MODEL")
    require("MILES_PROVIDER_TEMPERATURE" in chat_cpp, "ChatController must inject MILES_PROVIDER_TEMPERATURE")
    require("MILES_PROVIDER_MAX_TOKENS" in chat_cpp, "ChatController must inject MILES_PROVIDER_MAX_TOKENS")
    require("restartSidecar" in chat_h and "restartSidecar" in chat_cpp, "ChatController must expose restartSidecar()")
    require(
        "handleSettingsSaved" in chat_h and "handleSettingsSaved" in chat_cpp,
        "ChatController must respond to saved settings",
    )

    # main.cpp wiring
    require("setOrganizationName" in main_cpp, "main must set QCoreApplication organization name")
    require("setApplicationName" in main_cpp, "main must set QCoreApplication application name")
    require("SettingsService" in main_cpp, "main must construct SettingsService")
    require("SettingsController" in main_cpp, "main must construct SettingsController")
    require("loadFromModule" in main_cpp and "SettingsWindow" in main_cpp, "main must load SettingsWindow QML")

    # Tests
    require("InMemorySecretStore" in smoke, "smoke test must define InMemorySecretStore")
    require("msPerChar" in smoke, "smoke test must cover msPerChar persistence")
    require("apiKey" in smoke, "smoke test must cover apiKey routing through SecretStore")
    require("SettingsServiceSmoke" in desktop_cmake, "desktop CMake must register SettingsServiceSmoke target")
    require("settings_service_smoke" in desktop_cmake, "desktop CMake must register settings_service_smoke test")
    require(
        '"-framework Security"' in desktop_cmake or "Security" in desktop_cmake,
        "desktop CMake must link Apple Security framework on macOS",
    )
    require("MacSecretStore.mm" in desktop_cmake, "desktop CMake must list MacSecretStore.mm under the APPLE block")
    require("SettingsWindow.qml" in desktop_cmake, "desktop CMake must add SettingsWindow.qml to the QML module")
    require("check_phase_2_2_settings" in root_cmake, "root CMake must register Phase 2.2 contract check")

    # Docs
    require("Phase 2.2" in stage_doc, "Phase 2.2 stage record must exist")
    require("Phase 2.2" in index_doc, "doc index must link Phase 2.2 record")

    print("phase 2.2 settings contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
