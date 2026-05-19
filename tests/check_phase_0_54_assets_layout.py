#!/usr/bin/env python3
"""检查 Miles 皮肤资源已经迁入 skin assets 目录。"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QRC_PATH = ROOT / "apps/desktop/resources/pet_assets.qrc"
SKIN_ASSETS = ROOT / "apps/desktop/resources/skins/miles-edgeworth/assets"


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def qrc_aliases_by_prefix() -> dict[str, dict[str, str]]:
    tree = ET.parse(QRC_PATH)
    result: dict[str, dict[str, str]] = {}

    for resource in tree.getroot().findall("qresource"):
        prefix = resource.attrib.get("prefix", "")
        result[prefix] = {}
        for file_node in resource.findall("file"):
            alias = file_node.attrib.get("alias", "")
            source = (file_node.text or "").strip()
            result[prefix][alias] = source

    return result


def require_aliases(aliases: dict[str, str], required: list[str], prefix: str) -> None:
    for alias in required:
        require(alias in aliases, f"{prefix} 缺少资源 alias：{alias}")


def main() -> int:
    qrc_text = QRC_PATH.read_text(encoding="utf-8")
    qrc = qrc_aliases_by_prefix()
    pet_aliases = qrc.get("/pet", {})
    audio_aliases = qrc.get("/audio", {})

    require(SKIN_ASSETS.is_dir(), "Miles 皮肤应拥有 assets 目录")
    require("../../../gifs/" not in qrc_text, "pet_assets.qrc 不应继续引用根目录 gifs/")
    require("../../../audios/" not in qrc_text, "pet_assets.qrc 不应继续引用根目录 audios/")
    require("../../../prosbadge.png" not in qrc_text, "检察官徽章应迁入皮肤 assets/props/")

    for alias, source in pet_aliases.items():
        if alias == "manifest.json":
            continue
        require(
            source.startswith("skins/miles-edgeworth/assets/"),
            f"pet alias {alias} 应从皮肤 assets 目录加载，当前为 {source}",
        )
        require((QRC_PATH.parent / source).is_file(), f"pet alias {alias} 指向的文件不存在：{source}")
        require(
            not re.search(r"/(?:stand|walk|run|once)/\\d+\\.gif$", source),
            f"pet alias {alias} 仍使用旧数字 GIF 文件名：{source}",
        )

    for alias, source in audio_aliases.items():
        require(
            source.startswith("skins/miles-edgeworth/assets/audio/"),
            f"audio alias {alias} 应从皮肤 assets/audio 目录加载，当前为 {source}",
        )
        require((QRC_PATH.parent / source).is_file(), f"audio alias {alias} 指向的文件不存在：{source}")

    require_aliases(
        pet_aliases,
        [
            "stand-right.gif",
            "stand-left.gif",
            "walk-east.gif",
            "walk-west.gif",
            "walk-northEast.gif",
            "walk-northWest.gif",
            "walk-southEast.gif",
            "walk-southWest.gif",
            "walk-north.gif",
            "walk-south.gif",
            "run-east.gif",
            "run-west.gif",
            "run-northEast.gif",
            "run-northWest.gif",
            "run-southEast.gif",
            "run-southWest.gif",
            "run-north.gif",
            "run-south.gif",
            "idle-thinking-once-right.gif",
            "idle-tapping-head-right.gif",
            "idle-shrug-right.gif",
            "sleep-right.gif",
            "sleeping-right.gif",
            "wake-right.gif",
            "tea-right.gif",
            "prosecutor-badge.png",
        ],
        "/pet",
    )
    require_aliases(
        audio_aliases,
        [
            "holdit0.wav",
            "takethat0.wav",
            "objection0.wav",
            "eureka0.wav",
        ],
        "/audio",
    )

    readme = read("README.md")
    require("`gifs/`：Miles 桌宠动画素材。" not in readme, "README 不应把 gifs/ 描述为当前主资源目录")
    require("`audios/`：旧版语音素材" not in readme, "README 不应把 audios/ 描述为当前主资源目录")

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_54_assets_layout" in root_cmake, "CTest 未注册 Phase 0.54 资源布局检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
