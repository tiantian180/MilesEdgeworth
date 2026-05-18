#!/usr/bin/env python3
"""检查 Phase 0.15 的旧版菜单设置骨架。

旧版右键菜单里有静音、语音语言和禁止走动。v2 这一阶段先恢复这三项：
静音不再触发 SoundEffect，多语言语音从 manifest 选择对应 wav，禁止自动移动
让 walk / run 动画仍可播放但不再推动窗口。
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    recipes = manifest.get("recipes", {})

    expected_sounds = {
        "doubleClick.holdIt": {
            "jp": "qrc:/audio/holdit0.wav",
            "en": "qrc:/audio/holdit1.wav",
            "zh": "qrc:/audio/holdit2.wav",
        },
        "doubleClick.takeThat": {
            "jp": "qrc:/audio/takethat0.wav",
            "en": "qrc:/audio/takethat1.wav",
            "zh": "qrc:/audio/takethat2.wav",
        },
        "doubleClick.objection": {
            "jp": "qrc:/audio/objection0.wav",
            "en": "qrc:/audio/objection1.wav",
            "zh": "qrc:/audio/objection2.wav",
        },
        "doubleClick.eureka": {
            "jp": "qrc:/audio/eureka0.wav",
            "en": "qrc:/audio/eureka1.wav",
            # 旧版没有 eureka2，中文语音下不会进入 Eureka 分支；
            # v2 暂时用中文 objection 作为 fallback，避免出现日语兜底。
            "zh": "qrc:/audio/objection2.wav",
        },
    }

    for recipe_id, sounds in expected_sounds.items():
        recipe = recipes.get(recipe_id, {})
        require(recipe.get("sounds") == sounds, f"{recipe_id} 应声明三语言 sounds")

    qrc = read("apps/desktop/resources/pet_assets.qrc")
    for alias in [
        "holdit0.wav", "holdit1.wav", "holdit2.wav",
        "takethat0.wav", "takethat1.wav", "takethat2.wav",
        "objection0.wav", "objection1.wav", "objection2.wav",
        "eureka0.wav", "eureka1.wav",
    ]:
        require(alias in qrc, f"qrc 缺少语音资源 {alias}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(bool audioMuted",
        "Q_PROPERTY(QString voiceLanguage",
        "Q_PROPERTY(bool autoMovementEnabled",
        "audioMuted() const",
        "voiceLanguage() const",
        "autoMovementEnabled() const",
        "toggleAudioMuted",
        "setVoiceLanguage",
        "toggleAutoMovementEnabled",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "soundUrlForRecipe",
        "m_audioMuted",
        "m_voiceLanguage",
        "m_autoMovementEnabled",
        "if (m_audioMuted)",
        "if (!m_autoMovementEnabled)",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")

    qml = read("apps/desktop/qml/PetWindow.qml")
    for token in [
        "静音",
        "语音语言",
        "日语",
        "英语",
        "汉语",
        "禁止走动",
        "App.PetRuntime.toggleAudioMuted()",
        "App.PetRuntime.setVoiceLanguage(\"jp\")",
        "App.PetRuntime.setVoiceLanguage(\"en\")",
        "App.PetRuntime.setVoiceLanguage(\"zh\")",
        "App.PetRuntime.toggleAutoMovementEnabled()",
    ]:
        require(token in qml, f"PetWindow.qml 缺少 {token}")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_15_menu_audio_movement_settings" in root_cmake, "CTest 未注册 Phase 0.15 检查")


if __name__ == "__main__":
    main()
