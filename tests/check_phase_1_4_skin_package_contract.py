#!/usr/bin/env python3
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

print("phase 1.4 skin package contract ok")
