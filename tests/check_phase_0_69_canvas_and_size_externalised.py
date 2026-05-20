#!/usr/bin/env python3
"""检查 Phase 0.69 的 canvas、尺寸档位和 surface 去硬编码。"""

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
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    snapshot_h = read("apps/desktop/src/pet/runtime/RuntimeSnapshot.h")
    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    hit_zone_h = read("apps/desktop/src/pet/interaction/HitZoneMatcher.h")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")

    canvas = manifest.get("canvas", {})
    require(canvas.get("windowSize") == 120, "manifest.canvas.windowSize 应保留旧版基础窗口尺寸")
    require(canvas.get("imageSize") == 100, "manifest.canvas.imageSize 应保留旧版基础动画尺寸")
    require(canvas.get("hitZoneSize") == 240, "manifest.canvas.hitZoneSize 应声明单击命中逻辑画布")
    require(canvas.get("idleLoopAction") == "idle_stand", "manifest.canvas.idleLoopAction 应声明 idle loop 来源")

    sizes = manifest.get("sizes", [])
    size_by_id = {item.get("id"): item for item in sizes}
    expected_sizes = {
        "mini": ("迷你", 1.0),
        "small": ("小", 1.5),
        "medium": ("中", 2.0),
        "big": ("大", 3.0),
    }
    for size_id, (label, scale) in expected_sizes.items():
        item = size_by_id.get(size_id, {})
        require(item.get("label") == label, f"manifest.sizes 缺少 {size_id} 中文标签")
        require(item.get("scale") == scale, f"manifest.sizes 缺少 {size_id} 缩放值")
    require(manifest.get("defaultSize") == "medium", "manifest.defaultSize 应声明默认尺寸档位")

    audio = manifest.get("audio", {})
    require(audio.get("defaultVoiceLanguage") == "jp", "manifest.audio.defaultVoiceLanguage 应声明默认语音语言")

    for token in [
        "CanvasDefinition",
        "PetSizeDefinition",
        "AudioDefinition",
        "canvas",
        "hitZoneSize",
        "sizes",
        "defaultSizeId",
        "defaultVoiceLanguage",
    ]:
        require(token in manifest_h, f"SkinManifest 缺少字段：{token}")

    for token in [
        "manifest.canvas.windowSize",
        "manifest.canvas.imageSize",
        "manifest.canvas.hitZoneSize",
        "manifest.canvas.idleLoopActionId",
        "manifest.sizes.append",
        "manifest.defaultSizeId",
        "manifest.audio.defaultVoiceLanguage",
    ]:
        require(token in loader_cpp, f"SkinManifestLoader 未解析字段：{token}")

    for token in [
        "Q_PROPERTY(QString voiceLanguage",
        "voiceLanguage() const",
        "setVoiceLanguage",
        "voiceLanguageChanged",
        "m_voiceLanguage",
        "120.0",
        "100.0",
        'setPetSize("medium")',
        'if (sizeId == "mini")',
        'if (sizeId == "small")',
        'if (sizeId == "medium")',
        'if (sizeId == "big")',
        '"jp"',
        '"en"',
        '"zh"',
    ]:
        require(token not in runtime_h + runtime_cpp, f"PetRuntime 不应继续硬编码：{token}")

    require("voiceLanguage" not in snapshot_h, "RuntimeSnapshot 不应保留旧的 voiceLanguage 字段")
    require("manifest.canvas.windowSize * m_petScale" in runtime_h, "petWindowSize 应读取 manifest.canvas.windowSize")
    require("manifest.canvas.imageSize * m_petScale" in runtime_h, "petImageSize 应读取 manifest.canvas.imageSize")
    require("availablePetSizes" in runtime_h + runtime_cpp, "PetRuntime 应向原生菜单暴露 manifest sizes")
    require("currentActionAcceptsIdleLoopFinished" in runtime_h + runtime_cpp, "idle loop 判断应由 Runtime 读取 manifest 后提供")
    require("canvasWidth = 240.0" not in hit_zone_h, "HitZoneMatchContext 不应内置 Miles 240 宽度")
    require("canvasHeight = 240.0" not in hit_zone_h, "HitZoneMatchContext 不应内置 Miles 240 高度")
    require("manifest.canvas.hitZoneSize" in pipeline_cpp, "InteractionPipeline 应显式把 manifest hitZoneSize 注入命中计算")

    for token in [
        'QStringLiteral("迷你")',
        'QStringLiteral("小")',
        'QStringLiteral("中")',
        'QStringLiteral("大")',
        'QStringLiteral("mini")',
        'QStringLiteral("small")',
        'QStringLiteral("medium")',
        'QStringLiteral("big")',
    ]:
        require(token not in menu_cpp, f"PetContextMenu 不应硬编码尺寸档位：{token}")
    require("availablePetSizes" in menu_cpp, "PetContextMenu 应按 Runtime 提供的尺寸列表生成菜单")

    require("idle_stand" not in surface_cpp, "PetSurfaceWindow 不应写死 Miles idle action")
    require("currentActionAcceptsIdleLoopFinished()" in surface_cpp, "PetSurfaceWindow 应通过 Runtime 查询 idle loop 条件")

    guardrail = read("tests/check_phase_0_65_architecture_guardrails.cmake")
    for token in [
        "120.0",
        "100.0",
        '"mini"',
        '"small"',
        '"medium"',
        '"big"',
        "idle_stand",
    ]:
        require(token not in guardrail, f"Phase 0.69 已收口项不应继续留在 0.65 expected-fail 门禁：{token}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
