#!/usr/bin/env python3
"""检查 PetRuntime 的 manifest 数据结构已拆到独立模块。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    manifest_header = ROOT / "apps/desktop/src/pet/manifest/SkinManifest.h"
    runtime_header = read("apps/desktop/src/pet/PetRuntime.h")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")

    require(manifest_header.is_file(), "应新增 pet/manifest/SkinManifest.h")
    manifest_text = manifest_header.read_text(encoding="utf-8")

    for token in [
        "struct PhaseDefinition",
        "struct ActionDefinition",
        "struct RecipeDefinition",
        "struct ActionPoolDefinition",
        "struct BehaviorTriggerDefinition",
        "struct HitZoneDefinition",
        "struct PropDefinition",
        "struct SkinManifest",
    ]:
        require(token in manifest_text, f"SkinManifest.h 缺少 {token}")

    for token in [
        "struct PhaseDefinition",
        "struct ActionDefinition",
        "struct RecipeDefinition",
        "struct ActionPoolDefinition",
        "struct BehaviorTriggerDefinition",
        "struct HitZoneDefinition",
        "struct PropDefinition",
    ]:
        require(token not in runtime_header, f"PetRuntime.h 不应继续内联定义 {token}")

    require('#include "pet/manifest/SkinManifest.h"' in runtime_header, "PetRuntime.h 应 include SkinManifest.h")
    require("SkinManifest m_manifest;" in runtime_header, "PetRuntime 应持有 SkinManifest m_manifest")
    require("src/pet/manifest/SkinManifest.h" in desktop_cmake, "桌面 CMake 应列出 SkinManifest.h")
    require("check_phase_0_55_pet_runtime_manifest_split" in root_cmake, "CTest 未注册 Phase 0.55 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
