#!/usr/bin/env python3
"""Check the Phase 2.4.2 pre-cut clips contract."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Run: python3 -m pip install -r tools/requirements.txt") from exc

ROOT = Path(__file__).resolve().parents[1]
SKIN_ROOT = ROOT / "apps/desktop/resources/skins/miles-edgeworth"
MANIFEST_PATH = SKIN_ROOT / "manifest.json"
CLIPS_ROOT = SKIN_ROOT / "generated/clips"
SPLIT_SCRIPT = ROOT / "tools/split_manifest_clips.py"

EXPECTED_CLIPS = {
    "thinking.enter.right": [1, 4],
    "thinking.enter.left": [1, 4],
    "thinking.loop.right": [5, 8],
    "thinking.loop.left": [5, 8],
    "thinking.exit.right": [44, 47],
    "thinking.exit.left": [44, 47],
    "talking.enter.right": [1, 4],
    "talking.enter.left": [1, 4],
    "talking.loop.right": [5, 8],
    "talking.loop.left": [5, 8],
    "talking.exit.right": [9, 11],
    "talking.exit.left": [9, 11],
}

DISPATCH_SECTIONS = [
    "actions",
    "recipes",
    "behaviorTriggers",
    "behaviorRules",
    "skinCommands",
    "animationPools",
]
OLD_DISPATCH_TYPES = {"pool", "recipe", "action", "none", "returnToIdle", "toggleFacing"}


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def path_label(path: list[str]) -> str:
    return ".".join(path)


def diff_message(label: str, expected: set[str] | list[str], actual: set[str] | list[str]) -> str:
    expected_set = set(expected)
    actual_set = set(actual)
    missing = sorted(expected_set - actual_set)
    extra = sorted(actual_set - expected_set)
    parts = [label]
    if missing:
        parts.append("missing=" + ",".join(missing))
    if extra:
        parts.append("extra=" + ",".join(extra))
    return "; ".join(parts)


def gif_frame_durations(path: Path) -> list[int]:
    with Image.open(path) as image:
        return [frame.info.get("duration", 0) for frame in ImageSequence.Iterator(image)]


def resolve_skin_file(source: str) -> Path:
    relative = Path(source[len("file:"):])
    require(not relative.is_absolute(), f"clip source must be relative: {source}")
    root = SKIN_ROOT.resolve()
    resolved = (SKIN_ROOT / relative).resolve()
    try:
        resolved.relative_to(root)
    except ValueError:
        raise AssertionError(f"clip source escapes skin root: {source}") from None
    return resolved


def walk_json(value: Any, path: list[str]):
    if isinstance(value, dict):
        for key, item in value.items():
            key_path = path + [str(key)]
            yield key_path, item
            yield from walk_json(item, key_path)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            index_path = path + [str(index)]
            yield index_path, item
            yield from walk_json(item, index_path)


def find_legacy_dispatch_types(section_name: str, value: Any) -> list[str]:
    bad_paths = []
    for path, item in walk_json(value, [section_name]):
        if path[-1] == "type" and isinstance(item, str) and item in OLD_DISPATCH_TYPES:
            bad_paths.append(path_label(path))
    return bad_paths


def assert_manifest_contract(manifest_text: str, manifest: dict[str, Any]) -> None:
    require(manifest.get("schemaVersion") == 4, "manifest schemaVersion must be 4")
    for token in ["skin:", '"actionPools"', '"animation"']:
        require(token not in manifest_text, f"manifest must not contain legacy token {token}")

    animation_pools = manifest.get("animationPools")
    clips = manifest.get("clips")
    require(isinstance(animation_pools, dict), "manifest must contain top-level animationPools")
    require(isinstance(clips, dict), "manifest must contain top-level clips")
    expected_clip_ids = set(EXPECTED_CLIPS.keys())
    actual_clip_ids = set(clips.keys())
    require(
        expected_clip_ids <= actual_clip_ids,
        diff_message("manifest missing required thinking/talking clips", expected_clip_ids, actual_clip_ids),
    )

    for clip_id, frame_range in EXPECTED_CLIPS.items():
        clip = clips.get(clip_id)
        require(isinstance(clip, dict), f"{clip_id} must be an object")
        require(
            isinstance(clip.get("source"), str) and clip["source"].startswith("file:"),
            f"{clip_id} source must use file:",
        )
        require(clip.get("frameRange") == frame_range, f"{clip_id} frameRange must be {frame_range}")

    for clip_id, clip in clips.items():
        require(isinstance(clip, dict), f"{clip_id} must be an object")
        require(
            isinstance(clip.get("source"), str) and clip["source"].startswith("file:"),
            f"{clip_id} source must use file:",
        )
        require(isinstance(clip.get("frameRange"), list), f"{clip_id} must declare frameRange")

    variant_paths_missing_clip = []
    for path, item in walk_json(manifest.get("actions", {}), ["actions"]):
        if path[-1] == "variants" and isinstance(item, dict):
            for facing, variant in item.items():
                if isinstance(variant, dict) and "clip" not in variant:
                    variant_paths_missing_clip.append(path_label(path + [str(facing)]))
    require(
        not variant_paths_missing_clip,
        "action variants must use clip key: " + ", ".join(variant_paths_missing_clip),
    )

    legacy_dispatch_paths = []
    for section_name in DISPATCH_SECTIONS:
        section = manifest.get(section_name, {})
        legacy_dispatch_paths.extend(find_legacy_dispatch_types(section_name, section))
    require(
        not legacy_dispatch_paths,
        "manifest v4 dispatch sections must not use old type dispatch values: "
        + ", ".join(legacy_dispatch_paths),
    )

    bad_frame_fields = []
    for path, _ in walk_json(manifest, []):
        key = path[-1]
        allowed_top_clip_frame_range = len(path) == 3 and path[0] == "clips" and key == "frameRange"
        if key == "frameRange" and not allowed_top_clip_frame_range:
            bad_frame_fields.append(path_label(path))
        if key in {"frameStart", "frameEnd"}:
            bad_frame_fields.append(path_label(path))
    require(
        not bad_frame_fields,
        "runtime/action variants must not declare frameRange/frameStart/frameEnd: "
        + ", ".join(bad_frame_fields),
    )


def assert_source_contract() -> None:
    surface_text = read("apps/desktop/src/pet/surface/PetSurfaceWindow.h") + read(
        "apps/desktop/src/pet/surface/PetSurfaceWindow.cpp"
    )
    for token in [
        "loadManualFrameRange",
        "m_manualFrames",
        "m_manualFramePlayback",
        "m_manualFrameTimer",
        "QImageReader",
        "setPixmap(m_manualFrames",
    ]:
        require(token not in surface_text, f"PetSurfaceWindow must not contain runtime frame playback token {token}")

    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    for token in ["currentFrameStart", "currentFrameEnd", "m_currentFrameStart", "m_currentFrameEnd"]:
        require(token not in runtime_h, f"PetRuntime.h must not expose {token}")

    manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
    for token in ["frameStart", "frameEnd", "hasFrameRange"]:
        require(not re.search(rf"\b{token}\b", manifest_h), f"SkinManifest.h must not keep runtime {token}")
    for token in ["sourceFrameStart", "sourceFrameEnd"]:
        require(token in manifest_h, f"SkinManifest.h must keep build-time {token}")

    loader_cpp = read("apps/desktop/src/pet/manifest/SkinManifestLoader.cpp")
    skin_path_utils_h = read("apps/desktop/src/pet/manifest/SkinPathUtils.h")
    require("generatedClipUrlForId" in loader_cpp, "SkinManifestLoader.cpp missing generatedClipUrlForId")
    for token in ["existingLocalFileIsInsideRoot", "canonicalRootPath"]:
        require(token in skin_path_utils_h, f"SkinPathUtils.h missing {token}")

    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    for token in ["GenerateMilesClips", "split_manifest_clips.py"]:
        require(token in desktop_cmake, f"apps/desktop/CMakeLists.txt missing {token}")
    require("clips.qrc" not in desktop_cmake, "apps/desktop/CMakeLists.txt must not wire generated clips qrc")

    root_cmake = read("CMakeLists.txt")
    require(
        "check_phase_2_4_2_precut_clips" in root_cmake,
        "root CMakeLists.txt must register check_phase_2_4_2_precut_clips",
    )


def run_split_tool() -> None:
    result = subprocess.run(
        [sys.executable, str(SPLIT_SCRIPT), str(SKIN_ROOT)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(
        result.returncode == 0,
        "split_manifest_clips.py failed\nstdout:\n"
        + result.stdout
        + "\nstderr:\n"
        + result.stderr,
    )


def assert_generated_clips(clips: dict[str, Any]) -> None:
    require(CLIPS_ROOT.exists(), "generated/clips directory must exist after running splitter")
    actual = sorted(p.name for p in CLIPS_ROOT.glob("*.gif"))
    expected = sorted(f"{clip_id}.gif" for clip_id in clips)
    require(actual == expected, diff_message("generated clip files must exactly match manifest clips", expected, actual))
    require(not (SKIN_ROOT / "generated" / "clips.qrc").exists(), "splitter must not create generated/clips.qrc")

    for clip_id, clip in clips.items():
        source_path = resolve_skin_file(clip["source"])
        start, end = clip["frameRange"]
        expected_durations = gif_frame_durations(source_path)[start - 1:end]
        actual_durations = gif_frame_durations(CLIPS_ROOT / f"{clip_id}.gif")
        require(
            len(actual_durations) == end - start + 1,
            f"{clip_id} generated frame count must equal closed frameRange length",
        )
        require(
            actual_durations == expected_durations,
            f"{clip_id} generated frame durations must match source frameRange",
        )


def main() -> int:
    manifest_text = MANIFEST_PATH.read_text(encoding="utf-8")
    manifest = json.loads(manifest_text)
    assert_manifest_contract(manifest_text, manifest)
    assert_source_contract()
    run_split_tool()
    assert_generated_clips(manifest["clips"])

    print("phase 2.4.2 precut clips contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
