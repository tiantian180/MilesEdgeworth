#!/usr/bin/env python3
"""检查 PetRuntime 第一批稳定职责已经拆到独立服务。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    runtime_skin_cpp = read("apps/desktop/src/pet/PetRuntimeSkin.cpp")
    pipeline_cpp = read("apps/desktop/src/pet/interaction/InteractionPipeline.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")

    loader_header = ROOT / "apps/desktop/src/pet/manifest/SkinManifestLoader.h"
    loader_cpp = ROOT / "apps/desktop/src/pet/manifest/SkinManifestLoader.cpp"
    pool_selector_header = ROOT / "apps/desktop/src/pet/selection/AnimationPoolSelector.h"
    pool_selector_cpp = ROOT / "apps/desktop/src/pet/selection/AnimationPoolSelector.cpp"
    trigger_engine_header = ROOT / "apps/desktop/src/pet/behavior/BehaviorTriggerEngine.h"
    trigger_engine_cpp = ROOT / "apps/desktop/src/pet/behavior/BehaviorTriggerEngine.cpp"
    require(loader_header.is_file(), "应新增 SkinManifestLoader.h")
    require(loader_cpp.is_file(), "应新增 SkinManifestLoader.cpp")
    require(pool_selector_header.is_file(), "应新增 AnimationPoolSelector.h")
    require(pool_selector_cpp.is_file(), "应新增 AnimationPoolSelector.cpp")
    require(trigger_engine_header.is_file(), "应新增 BehaviorTriggerEngine.h")
    require(trigger_engine_cpp.is_file(), "应新增 BehaviorTriggerEngine.cpp")

    loader_text = loader_header.read_text(encoding="utf-8") + loader_cpp.read_text(encoding="utf-8")
    require("class SkinManifestLoader" in loader_text, "SkinManifestLoader 应作为独立加载器存在")
    require("loadFromResource" in loader_text, "SkinManifestLoader 应提供资源路径加载入口")
    require("fallbackManifest" in loader_text, "SkinManifestLoader 应提供 fallback manifest")

    pool_selector_text = pool_selector_header.read_text(encoding="utf-8") + pool_selector_cpp.read_text(encoding="utf-8")
    trigger_engine_text = trigger_engine_header.read_text(encoding="utf-8") + trigger_engine_cpp.read_text(encoding="utf-8")
    require("class AnimationPoolSelector" in pool_selector_text, "AnimationPoolSelector 应作为独立候选池选择器存在")
    require("resolvePoolId" in pool_selector_text, "AnimationPoolSelector 应处理语言覆盖池选择")
    require("selectEntry" in pool_selector_text, "AnimationPoolSelector 应处理候选池权重抽取")
    require("class BehaviorTriggerEngine" in trigger_engine_text, "BehaviorTriggerEngine 应作为独立触发规则执行器存在")
    require("BehaviorTriggerContext" in trigger_engine_text, "BehaviorTriggerEngine 应使用显式上下文匹配当前运行状态")
    require("matches" in trigger_engine_text, "BehaviorTriggerEngine 应处理触发条件匹配")
    require("selectEntry" in trigger_engine_text, "BehaviorTriggerEngine 应处理触发结果权重抽取")

    for token in [
        "void loadManifest()",
        "void loadFallbackManifest()",
        "void PetRuntime::loadManifest()",
        "void PetRuntime::loadFallbackManifest()",
        "QJsonDocument::fromJson",
        "root.value(\"actions\")",
        "root.value(\"recipes\")",
        "actionPoolIdForContext",
        "selectActionPoolEntry",
        "selectBehaviorTriggerEntry",
        "behaviorTriggerMatchesCurrentContext",
    ]:
        require(token not in runtime_header + runtime_cpp, f"PetRuntime 不应继续承担 manifest JSON 解析：{token}")

    runtime_sources = runtime_cpp + runtime_skin_cpp
    require("SkinManifestLoader::loadFromResource" in runtime_sources or "SkinManifestLoader::loadFromDescriptor" in runtime_sources, "PetRuntime 应调用 SkinManifestLoader 加载 manifest")
    require("SkinManifestLoader::fallbackManifest" in runtime_sources, "PetRuntime 应调用 SkinManifestLoader fallback")
    require("AnimationPoolSelector::resolvePoolId" in runtime_cpp, "PetRuntime 应委托 AnimationPoolSelector 解析候选池")
    require("AnimationPoolSelector::selectEntry" in runtime_cpp, "PetRuntime 应委托 AnimationPoolSelector 权重抽取")
    require("BehaviorTriggerEngine::matches" in pipeline_cpp, "InteractionPipeline 应委托 BehaviorTriggerEngine 匹配触发条件")
    require("BehaviorTriggerEngine::selectEntry" in pipeline_cpp, "InteractionPipeline 应委托 BehaviorTriggerEngine 权重抽取")
    require("src/pet/manifest/SkinManifestLoader.cpp" in desktop_cmake, "桌面 CMake 应编译 SkinManifestLoader.cpp")
    require("src/pet/manifest/SkinManifestLoader.h" in desktop_cmake, "桌面 CMake 应列出 SkinManifestLoader.h")
    require("src/pet/PetRuntimeSkin.cpp" in desktop_cmake, "桌面 CMake 应编译 PetRuntimeSkin.cpp")
    require("src/pet/selection/AnimationPoolSelector.cpp" in desktop_cmake, "桌面 CMake 应编译 AnimationPoolSelector.cpp")
    require("src/pet/selection/AnimationPoolSelector.h" in desktop_cmake, "桌面 CMake 应列出 AnimationPoolSelector.h")
    require("src/pet/behavior/BehaviorTriggerEngine.cpp" in desktop_cmake, "桌面 CMake 应编译 BehaviorTriggerEngine.cpp")
    require("src/pet/behavior/BehaviorTriggerEngine.h" in desktop_cmake, "桌面 CMake 应列出 BehaviorTriggerEngine.h")
    require("check_phase_0_56_pet_runtime_service_split" in root_cmake, "CTest 未注册 Phase 0.56 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
