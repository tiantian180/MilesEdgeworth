#!/usr/bin/env python3
"""Check the Phase 2.2 model config JSON contract."""

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
    provider_config_h = read("apps/desktop/src/settings/ProviderConfigFile.h")
    provider_config_cpp = read("apps/desktop/src/settings/ProviderConfigFile.cpp")
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
    design_doc = read("docs/v2/设计方案/模型配置与密钥存储设计.md")

    # providers.json storage
    require("class ProviderConfigFile" in provider_config_h, "ProviderConfigFile class missing")
    require("enum class LoadStatus" in provider_config_h, "ProviderConfigFile must expose LoadStatus")
    require("struct ModelConfig" in provider_config_h, "ProviderConfigFile must expose ModelConfig")
    require("std::optional<double>" in provider_config_h, "temperature must be optional")
    require("std::optional<int>" in provider_config_h, "maxTokens must be optional")
    require("QJsonObject extraFields" in provider_config_h, "unknown config fields must be preserved")
    require("QSaveFile" in provider_config_cpp, "providers.json writes must use QSaveFile")
    require("providers.json" in provider_config_cpp, "default file name must be providers.json")
    require("QStandardPaths::AppDataLocation" in provider_config_cpp,
            "default providers.json path must use AppDataLocation")
    require("setPermissions" in provider_config_cpp, "providers.json save must set owner-only permissions when possible")
    require("providers.json.bak" in provider_config_cpp, "invalid JSON must be backed up")
    require("modelConfigs" in provider_config_cpp, "providers.json must store modelConfigs")
    require("activeModelConfig" in provider_config_cpp, "providers.json must store activeModelConfig")
    require("moveConfig" in provider_config_h, "ProviderConfigFile must preserve user ordering")

    # SettingsService
    require("class SettingsService" in settings_h, "SettingsService class missing")
    require("ProviderConfigFile" in settings_h + settings_cpp,
            "SettingsService must persist model settings through ProviderConfigFile")
    require("SecretStore" not in settings_h + settings_cpp,
            "SettingsService must not depend on SecretStore")
    require("QSettings" not in settings_cpp,
            "provider settings must not be written to QSettings")
    for token in ["configNames", "activeModelConfig", "providerConfigured", "msPerChar", "save"]:
        require(token in settings_h, f"SettingsService missing {token}")

    # SettingsController + QML window
    require("class SettingsController" in controller_h, "SettingsController class missing")
    require("Q_INVOKABLE" in controller_h, "SettingsController must expose Q_INVOKABLE methods")
    require("openWindow" in controller_h, "SettingsController.openWindow() required")
    for token in [
        "configNames",
        "activeModelConfig",
        "configName",
        "baseUrl",
        "apiKey",
        "model",
        "temperatureText",
        "maxTokensText",
        "validationError",
        "providerConfigured",
    ]:
        require(token in controller_h + controller_cpp + settings_qml,
                f"SettingsController/QML missing {token}")
    require("SettingsControllerForeign" in controller_h, "SettingsControllerForeign QML singleton boilerplate required")
    require("QJSEngine::setObjectOwnership" in controller_h, "SettingsController singleton must keep C++ ownership")
    require("ApplicationWindow" in settings_qml, "SettingsWindow.qml must be ApplicationWindow")
    require("echoMode" in settings_qml, "API key field must use echoMode for masking")
    require("App.SettingsController.apiKey = apiKeyField.text" in settings_qml,
            "Settings save button must explicitly read the API key field")
    require("新增" in settings_qml, "SettingsWindow must allow adding model configs")
    require("删除" in settings_qml, "SettingsWindow must allow deleting model configs")

    # Chat UI and controller wiring
    require("设置" in chat_qml, "ChatWindow must surface a 设置 button")
    require("SettingsController" in chat_qml, "ChatWindow must reference the SettingsController singleton")
    require("TextEdit" in chat_qml, "chat message text must be selectable")
    require("selectByMouse: true" in chat_qml, "chat message text must support mouse selection")
    require("readOnly: true" in chat_qml, "chat message text selection must not make bubbles editable")
    require("providerConfigured" in chat_h + chat_cpp + chat_qml,
            "ChatWindow must know whether provider settings are complete")
    require("sidecarReady && providerConfigured" in chat_qml,
            "ChatWindow input must be disabled until the sidecar and provider config are ready")
    require("先点设置填写模型配置" in chat_qml,
            "disabled input placeholder must tell the user to fill model config")
    require("SettingsService" in chat_h, "ChatController must take a SettingsService")
    require("providerConfiguredChanged" in chat_h, "ChatController must notify provider config completeness")
    require("QProcessEnvironment" in chat_cpp, "ChatController must build a QProcessEnvironment")
    require("MILES_PROVIDER_BASE_URL" in chat_cpp, "ChatController must inject MILES_PROVIDER_BASE_URL")
    require("MILES_PROVIDER_API_KEY" in chat_cpp, "ChatController must inject MILES_PROVIDER_API_KEY")
    require("MILES_PROVIDER_MODEL" in chat_cpp, "ChatController must inject MILES_PROVIDER_MODEL")
    require("MILES_PROVIDER_TEMPERATURE" in chat_cpp, "ChatController must inject optional temperature")
    require("MILES_PROVIDER_MAX_TOKENS" in chat_cpp, "ChatController must inject optional maxTokens")
    require("未配置模型" in chat_cpp, "ChatController status must expose missing model config")

    # main.cpp wiring and build
    require("setOrganizationName" in main_cpp, "main must set QCoreApplication organization name")
    require("setApplicationName" in main_cpp, "main must set QCoreApplication application name")
    require("SettingsService" in main_cpp, "main must construct SettingsService")
    require("SettingsController" in main_cpp, "main must construct SettingsController")
    require("SecretStore" not in main_cpp, "main must not construct SecretStore")
    require("loadFromModule" in main_cpp and "SettingsWindow" in main_cpp, "main must load SettingsWindow QML")
    require("ProviderConfigFile.cpp" in desktop_cmake, "desktop CMake must compile ProviderConfigFile.cpp")
    require("SecretStore" not in desktop_cmake, "desktop CMake must not compile SecretStore")
    require("MacSecretStore" not in desktop_cmake, "desktop CMake must not compile MacSecretStore")
    require("Security" not in desktop_cmake, "desktop CMake must not link the Apple Security framework")
    require("SettingsServiceSmoke" in desktop_cmake, "desktop CMake must register SettingsServiceSmoke target")
    require("settings_service_smoke" in desktop_cmake, "desktop CMake must register settings_service_smoke test")
    require("SettingsWindow.qml" in desktop_cmake, "desktop CMake must add SettingsWindow.qml to the QML module")
    require("SettingsLogging.cpp" in desktop_cmake, "desktop CMake must compile settings logging category")
    require("check_phase_2_2_settings" in root_cmake, "root CMake must register Phase 2.2 contract check")

    # Tests and docs
    require("ProviderConfigFile" in smoke, "settings smoke test must cover ProviderConfigFile")
    require("providers.json.bak" in smoke, "settings smoke test must cover invalid JSON backup")
    require("extraFields" in smoke, "settings smoke test must cover unknown field preservation")
    require("msPerChar" in smoke, "smoke test must cover msPerChar persistence")
    require("apiKey" in smoke, "smoke test must cover apiKey persistence in providers.json")
    require("Phase 2.2" in stage_doc, "Phase 2.2 stage record must exist")
    require("Phase 2.2" in index_doc, "doc index must link Phase 2.2 record")
    require("providers.json" in design_doc and "ProviderConfigFile" in design_doc,
            "design doc must describe providers.json and ProviderConfigFile")

    print("phase 2.2 model config JSON contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
