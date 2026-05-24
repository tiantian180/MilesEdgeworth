#!/usr/bin/env python3
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)

loader_h = read("apps/desktop/src/pet/manifest/SkinManifestLoader.h")
manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
qrc = read("apps/desktop/resources/pet_assets.qrc")
qrc_path = ROOT / "apps/desktop/resources/pet_assets.qrc"
skin_assets = ROOT / "apps/desktop/resources/skins/miles-edgeworth/assets"

tree = ET.parse(qrc_path)
resources = {
    resource.attrib.get("prefix", ""): {
        file_node.attrib.get("alias", ""): (file_node.text or "").strip()
        for file_node in resource.findall("file")
    }
    for resource in tree.getroot().findall("qresource")
}
pet_aliases = resources.get("/pet", {})
audio_aliases = resources.get("/audio", {})

require("SkinDescriptor.h" in loader_h, "SkinManifestLoader.h must include SkinDescriptor.h")
require("loadFromDirectory" in loader_h, "loader must expose loadFromDirectory()")
require("discoverAll" in loader_h, "loader must expose discoverAll()")
require("resolveSkinUrl" in loader_h, "loader must expose resolveSkinUrl() for tests and deterministic URL handling")
require("userSkinDirectoryPath" in loader_h, "loader must expose userSkinDirectoryPath()")
require("portableSkinDirectoryPath" in loader_h, "loader must expose portableSkinDirectoryPath()")

require("skinId" in manifest_h, "SkinManifest must store loaded skin id")
require("skinName" in manifest_h, "SkinManifest must store loaded skin display name")
require("skinRootUrl" in manifest_h, "SkinManifest must store resolved root URL")
require("builtin" in manifest_h, "SkinManifest must mark built-in skins")

require("availableSkins" in runtime_h, "PetRuntime must expose availableSkins")
require("activeSkinId" in runtime_h, "PetRuntime must expose activeSkinId")
require("setActiveSkin" in runtime_h, "PetRuntime must expose setActiveSkin")
require("reloadActiveSkin" in runtime_h, "PetRuntime must expose reloadActiveSkin")

require("皮肤" in menu_cpp, "context menu must expose a skin submenu")
require("打开皮肤目录" in menu_cpp, "context menu must expose the user skin directory")
require("重载当前皮肤" in menu_cpp, "context menu must expose skin reload")

require('prefix="/skins/miles-edgeworth"' in qrc, "qrc must expose built-in skin under /skins/miles-edgeworth")
require('alias="skin.json"' in qrc, "qrc must include built-in skin.json")
require('alias="manifest.json"' in qrc, "qrc must include built-in manifest.json under skin root")

# Phase 1.4 之后，Miles 的内置皮肤既要支持 file: URL，也要保持
# 旧 qrc:/pet 和 qrc:/audio alias 兼容层。这里集中守住资源目录契约，
# 避免继续保留 Phase 0.54 的历史阶段测试。
require(skin_assets.is_dir(), "Miles built-in skin must keep runtime assets under skins/miles-edgeworth/assets")
require("../../../gifs/" not in qrc, "pet_assets.qrc must not reference legacy root gifs/")
require("../../../audios/" not in qrc, "pet_assets.qrc must not reference legacy root audios/")
require("../../../prosbadge.png" not in qrc, "prosecutor badge must be loaded from skin assets/props/")

for alias, source in pet_aliases.items():
    if alias == "manifest.json":
        continue
    require(
        source.startswith("skins/miles-edgeworth/assets/"),
        f"pet alias {alias} must load from skin assets, got {source}",
    )
    require((qrc_path.parent / source).is_file(), f"pet alias {alias} points to missing file: {source}")
    require(
        not re.search(r"/(?:stand|walk|run|once)/\d+\.gif$", source),
        f"pet alias {alias} still uses legacy numeric GIF filename: {source}",
    )

for alias, source in audio_aliases.items():
    require(
        source.startswith("skins/miles-edgeworth/assets/audio/"),
        f"audio alias {alias} must load from skin assets/audio, got {source}",
    )
    require((qrc_path.parent / source).is_file(), f"audio alias {alias} points to missing file: {source}")

print("phase 1.4 skin package contract ok")
