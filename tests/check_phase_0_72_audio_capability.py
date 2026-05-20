#!/usr/bin/env python3
"""检查 Phase 0.72 的可选 Audio Capability。

语音语言是皮肤可选能力：Miles 皮肤声明语言列表，菜单按 manifest 生成；
通用 Runtime 只持有当前 audio language id，不写死 jp/en/zh。
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


def main() -> int:
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    audio = manifest.get("audio", {})
    languages = audio.get("voiceLanguages", [])
    language_by_id = {item.get("id"): item for item in languages if isinstance(item, dict)}

    require(audio.get("defaultVoiceLanguage") == "jp", "Miles 默认语音语言应为 jp")
    require({"jp", "en", "zh"} <= set(language_by_id), "Miles 应声明 jp/en/zh 三个可选语音语言")
    require(language_by_id["jp"].get("label") == "日语", "jp 语言菜单标签应为日语")
    require(language_by_id["en"].get("label") == "英语", "en 语言菜单标签应为英语")
    require(language_by_id["zh"].get("label") == "中文", "zh 语言菜单标签应为中文")

    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    audio_h = read("apps/desktop/src/pet/effects/AudioController.h")
    audio_cpp = read("apps/desktop/src/pet/effects/AudioController.cpp")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    selector_cpp = read("apps/desktop/src/pet/selection/ActionPoolSelector.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    smoke_test = read("apps/desktop/tests/pet_runtime_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    phase_record = read("docs/v2/阶段记录/第0阶段桌面壳验证.md")
    split_design = read("docs/v2/设计方案/桌宠运行时职责拆分设计.md")
    playback_design = read("docs/v2/设计方案/皮肤包播放行为设计.md")

    for token in [
        "struct AudioLanguageDefinition",
        "QList<AudioLanguageDefinition> voiceLanguages",
        "QString defaultVoiceLanguage",
    ]:
        require(token in manifest_h, f"SkinManifest 缺少 audio capability 字段：{token}")

    for token in [
        "audio.value(\"voiceLanguages\")",
        "AudioLanguageDefinition",
        "manifest.audio.voiceLanguages.append",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader 未解析 voiceLanguages：{token}")

    for token in [
        "class AudioController",
        "setAudioDefinition",
        "currentLanguageId",
        "availableLanguages",
        "setLanguage",
        "toggleMuted",
        "soundUrlForRecipe",
        "playSoundForRecipe",
        "playSound",
        "QVariantList",
        "m_audio.voiceLanguages.isEmpty() || hasLanguage",
    ]:
        require(token in audio_h + audio_cpp, f"AudioController 缺少能力：{token}")

    for token in [
        "Q_PROPERTY(QString currentAudioLanguageId",
        "Q_PROPERTY(QVariantList availableAudioLanguages",
        "currentAudioLanguageId() const",
        "availableAudioLanguages() const",
        "setAudioLanguage",
        "currentAudioLanguageChanged",
        "m_audioController",
    ]:
        require(token in runtime_h + runtime_cpp, f"PetRuntime 应通过 AudioController 暴露能力：{token}")

    for forbidden in [
        "m_audioMuted",
        "m_currentSoundUrl",
        "m_soundPlaybackSerial",
        '"jp"',
        '"en"',
        '"zh"',
    ]:
        require(forbidden not in runtime_h + runtime_cpp, f"PetRuntime 不应保留音频状态或语言硬编码：{forbidden}")

    require("resolvePoolId(m_manifest.actionPools, poolId, m_audioController.currentLanguageId())" in runtime_cpp, "候选池语言覆盖应读取 AudioController 当前语言")
    require("normalizedPoolId + \".\" + languageId" in selector_cpp, "ActionPoolSelector 应继续支持语言后缀池")

    for token in [
        "availableAudioLanguages",
        "currentAudioLanguageId",
        "setAudioLanguage",
        "语音",
        "QActionGroup",
    ]:
        require(token in menu_cpp, f"PetContextMenu 应按 Audio Capability 生成语音菜单：{token}")
    require("语音语言" not in menu_cpp, "菜单文案应收敛为能力名“语音”，不再叫临时语音语言")

    for token in [
        "runtime.availableAudioLanguages()",
        'runtime.setAudioLanguage("zh")',
        'runtime.currentSoundUrl().toString() == "qrc:/audio/holdit2.wav"',
        'runtime.setAudioLanguage("en")',
        'runtime.currentSoundUrl().toString() == "qrc:/audio/holdit1.wav"',
        'runtime.setAudioLanguage("jp")',
        "defaultOnlyAudio.defaultVoiceLanguage",
        "defaultOnlyAudioController.availableLanguages().isEmpty()",
    ]:
        require(token in smoke_test, f"PetRuntimeSmoke 缺少语音语言回归：{token}")

    for token in [
        "src/pet/effects/AudioController.cpp",
        "src/pet/effects/AudioController.h",
    ]:
        require(token in desktop_cmake, f"桌面 CMake 缺少 AudioController 文件：{token}")

    require("check_phase_0_72_audio_capability" in root_cmake, "CTest 未注册 Phase 0.72 检查")
    require("Phase 0.72" in phase_record, "阶段记录应登记 Phase 0.72")
    require("Audio Capability" in split_design and "Audio Capability" in playback_design, "长期设计文档应保留 Audio Capability 说明")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
