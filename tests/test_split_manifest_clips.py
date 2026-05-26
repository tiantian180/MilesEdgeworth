#!/usr/bin/env python3
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Run: python3 -m pip install -r tools/requirements.txt") from exc

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "split_manifest_clips.py"
TMP = ROOT / ".tmp_phase_2_4_2_clip_test"
MILES_THINKING_LEFT = ROOT / "apps/desktop/resources/skins/miles-edgeworth/assets/body/gestures/thinking-left.gif"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def make_source_gif(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = []
    for index in range(6):
        image = Image.new("RGBA", (4, 4), (index * 30, 0, 255 - index * 30, 255))
        frames.append(image)
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=[11, 22, 33, 44, 55, 66],
        loop=0,
    )


def make_duplicate_opaque_source_gif(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    palette = [0, 0, 0] * 256
    palette[0:3] = [10, 20, 30]
    palette[3:6] = [10, 20, 30]
    first = Image.new("P", (4, 4), 0)
    second = Image.new("P", (4, 4), 1)
    first.putpalette(palette)
    second.putpalette(palette)
    first.save(
        path,
        save_all=True,
        append_images=[second],
        duration=[20, 20],
        loop=0,
        optimize=False,
        disposal=2,
    )


def frame_count(path: Path) -> int:
    with Image.open(path) as image:
        return sum(1 for _ in ImageSequence.Iterator(image))


def frame_durations(path: Path) -> list[int]:
    with Image.open(path) as image:
        return [frame.info.get("duration", 0) for frame in ImageSequence.Iterator(image)]


def write_manifest(skin_root: Path, clips: dict) -> None:
    (skin_root / "manifest.json").write_text(json.dumps({"clips": clips}), encoding="utf-8")


def run_tool(skin_root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(skin_root)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def require_failure(skin_root: Path, expected_message: str) -> None:
    result = run_tool(skin_root)
    require(result.returncode != 0, f"expected failure containing {expected_message!r}")
    require(expected_message in result.stderr, result.stderr)
    require("Traceback" not in result.stderr, result.stderr)


def valid_clip(source: str = "file:assets/body/raw/source.gif", frame_range: Optional[list[int]] = None) -> dict:
    return {
        "source": source,
        "frameRange": frame_range or [2, 4],
    }


def main() -> None:
    if TMP.exists():
        shutil.rmtree(TMP)
    try:
        skin = TMP / "test-skin"
        source = skin / "assets" / "body" / "raw" / "source.gif"
        make_source_gif(source)

        write_manifest(skin, {
            "thinking.enter.right": valid_clip(frame_range=[1, 2]),
            "thinking.loop.right": valid_clip(frame_range=[3, 4]),
            "thinking.exit.right": valid_clip(frame_range=[5, 6]),
        })

        stale = skin / "generated" / "clips" / "stale.gif"
        stale.parent.mkdir(parents=True, exist_ok=True)
        stale.write_bytes(b"stale")
        stale_qrc = skin / "generated" / "clips.qrc"
        stale_qrc.write_text("<RCC />", encoding="utf-8")
        unrelated_qrc = skin / "generated" / "keep.qrc"
        unrelated_qrc.write_text("<RCC />", encoding="utf-8")

        result = run_tool(skin)
        require(result.returncode == 0, result.stderr)
        require("generated 3 clips under" in result.stdout, result.stdout)
        require("generated/clips" in result.stdout, result.stdout)

        generated = skin / "generated" / "clips"
        enter = generated / "thinking.enter.right.gif"
        loop = generated / "thinking.loop.right.gif"
        exit = generated / "thinking.exit.right.gif"
        require(enter.exists(), "enter clip should be generated")
        require(loop.exists(), "loop clip should be generated")
        require(exit.exists(), "exit clip should be generated")
        require(not stale.exists(), "script should clean stale generated clips before writing")
        require(not (skin / "generated" / "clips.qrc").exists(), "tool must not write clips.qrc")
        require(unrelated_qrc.exists(), "tool must not delete unrelated generated qrc files")
        require("clips.qrc" not in result.stdout, "tool output must not reference clips.qrc")
        source_durations = frame_durations(source)
        for clip_path, expected_durations in [
            (enter, source_durations[0:2]),
            (loop, source_durations[2:4]),
            (exit, source_durations[4:6]),
        ]:
            require(frame_count(clip_path) == 2, f"{clip_path.name} should generate 2 frames")
            require(
                frame_durations(clip_path) == expected_durations,
                f"{clip_path.name} should preserve source frame durations",
            )

        duplicate_skin = TMP / "duplicate-frame-skin"
        duplicate_source = duplicate_skin / "assets" / "body" / "gestures" / "thinking-left.gif"
        duplicate_source.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(MILES_THINKING_LEFT, duplicate_source)
        write_manifest(duplicate_skin, {
            "thinking.enter.left": valid_clip("file:assets/body/gestures/thinking-left.gif", [1, 4]),
        })
        result = run_tool(duplicate_skin)
        require(result.returncode == 0, result.stderr)
        duplicate_output = duplicate_skin / "generated" / "clips" / "thinking.enter.left.gif"
        expected_duplicate_durations = frame_durations(duplicate_source)[0:4]
        require(
            frame_count(duplicate_output) == 4,
            "generated clip should preserve duplicate source frames instead of merging durations",
        )
        require(
            frame_durations(duplicate_output) == expected_duplicate_durations,
            "generated duplicate-frame clip should preserve per-frame durations",
        )

        failure_skin = TMP / "failure-preserves-generated-skin"
        failure_source = failure_skin / "assets" / "body" / "raw" / "source.gif"
        make_source_gif(failure_source)
        existing_clip = failure_skin / "generated" / "clips" / "existing.gif"
        existing_clip.parent.mkdir(parents=True, exist_ok=True)
        existing_clip.write_bytes(b"existing")
        existing_qrc = failure_skin / "generated" / "clips.qrc"
        existing_qrc.write_text("<RCC />", encoding="utf-8")
        write_manifest(failure_skin, {
            "preserve.valid": valid_clip(),
            "preserve.zzz_missing": valid_clip("file:assets/body/raw/missing.gif"),
        })
        result = run_tool(failure_skin)
        require(result.returncode != 0, "expected missing source to fail")
        require("source file not found" in result.stderr, result.stderr)
        require("Traceback" not in result.stderr, result.stderr)
        require(existing_clip.exists(), "failed generation must not delete existing clips")
        require(existing_clip.read_bytes() == b"existing", "failed generation must not delete existing clips")
        require(existing_qrc.exists(), "failed generation must not delete existing clips.qrc")

        atomic_failure_skin = TMP / "atomic-failure-preserves-generated-skin"
        atomic_source = atomic_failure_skin / "assets" / "body" / "raw" / "source.gif"
        atomic_duplicate_source = atomic_failure_skin / "assets" / "body" / "raw" / "duplicate-opaque.gif"
        make_source_gif(atomic_source)
        make_duplicate_opaque_source_gif(atomic_duplicate_source)
        atomic_existing_clip = atomic_failure_skin / "generated" / "clips" / "existing.gif"
        atomic_existing_clip.parent.mkdir(parents=True, exist_ok=True)
        atomic_existing_clip.write_bytes(b"existing")
        write_manifest(atomic_failure_skin, {
            "atomic.valid": valid_clip(),
            "atomic.zzz_duplicate": valid_clip("file:assets/body/raw/duplicate-opaque.gif", [1, 2]),
        })
        result = run_tool(atomic_failure_skin)
        require(result.returncode != 0, "expected duplicate opaque clip generation to fail")
        require("duplicate opaque frames" in result.stderr, result.stderr)
        require(
            atomic_existing_clip.exists() and atomic_existing_clip.read_bytes() == b"existing",
            "failed write-time generation must preserve previous generated clips atomically",
        )

        (skin / "manifest.json").unlink()
        require_failure(skin, "manifest not found")

        (skin / "manifest.json").write_text("{", encoding="utf-8")
        require_failure(skin, "manifest is invalid JSON")

        write_manifest(skin, {
            "talking.loop.right": valid_clip("file:assets/body/raw/missing.gif"),
        })
        require_failure(skin, "source file not found")

        write_manifest(skin, {
            "talking.loop.right": valid_clip("file:../outside.gif"),
        })
        require_failure(skin, "source escapes skin root")

        write_manifest(skin, {
            "talking.loop.right": valid_clip(frame_range=[0, 1]),
        })
        require_failure(skin, "frameRange must be 1-based inclusive")

        write_manifest(skin, {
            "talking.loop.right": valid_clip(frame_range=[2, 8]),
        })
        require_failure(skin, "frameRange exceeds source frame count")

        corrupt = skin / "assets" / "body" / "raw" / "corrupt.gif"
        corrupt.write_bytes(b"not a gif")
        write_manifest(skin, {
            "talking.loop.right": valid_clip("file:assets/body/raw/corrupt.gif"),
        })
        require_failure(skin, "unable to read source GIF")

        write_manifest(skin, {
            "../escape": valid_clip(frame_range=[1, 1]),
        })
        require_failure(skin, "invalid clip name")
    finally:
        if TMP.exists():
            shutil.rmtree(TMP)

    print("phase 2.4.2 clip slicing tool ok")


if __name__ == "__main__":
    main()
