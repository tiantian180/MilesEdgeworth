#!/usr/bin/env python3
"""检查 Phase 0.15 的旧版设置骨架。

旧版右键菜单里有静音、语音语言和禁止走动。v2 原生菜单保留
静音和禁止走动；语音入口由 Phase 0.72 的可选 Audio Capability
根据皮肤 manifest 动态生成。
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
    skin_root = ROOT / "apps/desktop/resources/skins/miles-edgeworth"
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    recipes = manifest.get("recipes", {})

    expected_sounds = {
        "doubleClick.holdIt": {
            "jp": "file:assets/audio/voice/holdit0.wav",
            "en": "file:assets/audio/voice/holdit1.wav",
            "zh": "file:assets/audio/voice/holdit2.wav",
        },
        "doubleClick.takeThat": {
            "jp": "file:assets/audio/voice/takethat0.wav",
            "en": "file:assets/audio/voice/takethat1.wav",
            "zh": "file:assets/audio/voice/takethat2.wav",
        },
        "doubleClick.objection": {
            "jp": "file:assets/audio/voice/objection0.wav",
            "en": "file:assets/audio/voice/objection1.wav",
            "zh": "file:assets/audio/voice/objection2.wav",
        },
        "doubleClick.eureka": {
            "jp": "file:assets/audio/voice/eureka0.wav",
            "en": "file:assets/audio/voice/eureka1.wav",
            # 旧版没有 eureka2，中文语音下不会进入 Eureka 分支；
            # v2 暂时用中文 objection 作为 fallback，避免出现日语兜底。
            "zh": "file:assets/audio/voice/objection2.wav",
        },
    }

    for recipe_id, sounds in expected_sounds.items():
        recipe = recipes.get(recipe_id, {})
        require(recipe.get("sounds") == sounds, f"{recipe_id} 应声明三语言 sounds")
        for language_id, url in sounds.items():
            require((skin_root / url[len("file:"):]).is_file(), f"{recipe_id}.{language_id} 皮肤音频文件缺失 {url}")

    pet_runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in [
        "Q_PROPERTY(bool audioMuted",
        "Q_PROPERTY(bool autoMovementEnabled",
        "audioMuted() const",
        "autoMovementEnabled() const",
        "toggleAudioMuted",
        "toggleAutoMovementEnabled",
    ]:
        require(token in pet_runtime_h, f"PetRuntime.h 缺少 {token}")
    for token in [
        "Q_PROPERTY(QString voiceLanguage",
        "voiceLanguage() const",
        "setVoiceLanguage",
        "voiceLanguageChanged",
    ]:
        require(token not in pet_runtime_h, f"PetRuntime.h 不应保留临时语音语言入口：{token}")

    pet_runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    for token in [
        "m_audioController",
        "m_audioController.currentLanguageId()",
        "m_audioController.playSoundForRecipe",
        "m_audioController.toggleMuted",
        "m_autoMovementEnabled",
        "if (!m_autoMovementEnabled)",
    ]:
        require(token in pet_runtime_cpp, f"PetRuntime.cpp 缺少 {token}")
    for token in [
        "m_voiceLanguage",
        "setVoiceLanguage",
    ]:
        require(token not in pet_runtime_cpp, f"PetRuntime.cpp 不应保留临时语音语言状态：{token}")

    require(
        manifest.get("audio", {}).get("defaultVoiceLanguage") == "jp",
        "manifest 应声明 audio.defaultVoiceLanguage 作为默认语音语言",
    )
    require(
        {item.get("id") for item in manifest.get("audio", {}).get("voiceLanguages", [])} == {"jp", "en", "zh"},
        "manifest 应声明可选语音语言",
    )

    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    for token in [
        "静音",
        "禁止走动",
        "语音",
        "PetRuntime::toggleAudioMuted",
        "PetRuntime::toggleAutoMovementEnabled",
        "setAudioLanguage",
    ]:
        require(token in menu_cpp, f"PetContextMenu.cpp 缺少 {token}")

    require("语音语言" not in menu_cpp, "菜单文案应使用 Audio Capability 的“语音”入口")
    require("setVoiceLanguage" not in menu_cpp, "原生菜单不应继续暴露旧的 voiceLanguage 字段")
    require("m_soundEffect->setVolume(m_runtime->audioMuted() ? 0.0f : 0.8f)" in surface_cpp, "原生语音播放应立即响应静音状态")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_15_menu_audio_movement_settings" in root_cmake, "CTest 未注册 Phase 0.15 检查")


if __name__ == "__main__":
    main()
