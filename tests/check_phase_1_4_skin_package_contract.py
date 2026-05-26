#!/usr/bin/env python3
import json
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


skin_json = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/skin.json"))
manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
qrc_path = ROOT / "apps/desktop/resources/pet_assets.qrc"
qrc_tree = ET.parse(qrc_path)
loader_h = read("apps/desktop/src/pet/manifest/SkinManifestLoader.h")
loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
descriptor_h = read("apps/desktop/src/pet/manifest/SkinDescriptor.h")
runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
cmake = read("apps/desktop/CMakeLists.txt")

qresources = qrc_tree.getroot().findall("qresource")
prefixes = {resource.attrib.get("prefix", "") for resource in qresources}

allowed_prefixes = {"/icon"}
for forbidden_prefix in ["/pet", "/audio", "/skins/miles-edgeworth"]:
    require(forbidden_prefix not in prefixes, f"pet_assets.qrc must not keep skin qresource prefix {forbidden_prefix}")
require(prefixes <= allowed_prefixes, f"pet_assets.qrc prefixes must be app-only resources, got {sorted(prefixes)}")
require("/icon" in prefixes, "pet_assets.qrc may keep app icon resources under /icon")

for file_node in qrc_tree.getroot().iter("file"):
    alias = file_node.attrib.get("alias", "")
    source = (file_node.text or "").strip()
    for forbidden in [
        "skins/miles-edgeworth",
        "generated/clips",
        "assets/body",
        "assets/audio",
        "gifs/",
        "audios/",
        ".gif",
        ".wav",
    ]:
        require(
            forbidden not in alias and forbidden not in source,
            f"pet_assets.qrc must not embed skin resource token {forbidden}: alias={alias}, source={source}",
        )

require(skin_json.get("skinSchemaVersion") == 1, "skin.json must use skinSchemaVersion 1")
require(skin_json.get("id") == "miles-edgeworth", "official skin id must stay stable")
require(skin_json.get("manifest") == "manifest.json", "skin.json must point to manifest.json")
require(str(skin_json.get("thumbnail", "")).startswith("file:"), "thumbnail must use file: URL")
require(manifest.get("schemaVersion") == 4, "manifest.json must remain manifest schema v4")

for token in [
    "loadFromDirectory",
    "discoverAll",
    "resolveSkinUrl",
    "userSkinDirectoryPath",
    "appSkinDirectoryPath",
]:
    require(token in loader_h, f"SkinManifestLoader.h must expose {token}")
for token in [
    "skinId",
    "skinName",
    "skinRootUrl",
]:
    require(token in manifest_h, f"SkinManifest.h must expose {token}")
for token in [
    "availableSkins",
    "activeSkinId",
    "setActiveSkin",
    "reloadActiveSkin",
]:
    require(token in runtime_h, f"PetRuntime.h must expose {token}")
for token in [
    "皮肤",
    "打开皮肤目录",
    "重载当前皮肤",
]:
    require(token in menu_cpp, f"PetContextMenu.cpp must expose skin UI string {token}")

require("appSkinDirectoryPath" in loader_h, "loader must expose appSkinDirectoryPath()")
require("portableSkinDirectoryPath" not in loader_h + loader_cpp, "portableSkinDirectoryPath must be replaced")
require("loadFromResource" not in loader_h + loader_cpp, "qrc manifest loading entry must be removed")
require("SkinManifestLoader::discoverAll" in loader_cpp, "loader must still expose discoverAll")
require("qrc:/skins/miles-edgeworth" not in loader_cpp, "loader must not hardcode qrc built-in skin")
require("skinSchemaVersion" in descriptor_h + loader_cpp, "descriptor loading must validate skinSchemaVersion")
require("manifestVersion" not in descriptor_h + loader_cpp, "skin metadata must not use manifestVersion")
require("builtin" not in manifest_h + descriptor_h, "SkinManifest/SkinDescriptor must not expose builtin")

require("GenerateMilesClips" in cmake, "CMake must still generate Miles clips")
require("split_manifest_clips.py" in cmake, "CMake must call clip splitter")
require(
    "copy_directory" in cmake and "skins/miles-edgeworth" in cmake,
    "CMake must copy official skin beside executable",
)
require("clips.qrc" not in cmake, "generated clips qrc must not be part of CMake")
require(
    "SignMilesEdgeworthDesktopBundle" not in cmake,
    "desktop bundle must be signed once by the MilesEdgeworthDesktop POST_BUILD step",
)

print("phase 1.4 skin package contract ok")
