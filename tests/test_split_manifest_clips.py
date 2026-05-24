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


def frame_count(path: Path) -> int:
    with Image.open(path) as image:
        return sum(1 for _ in ImageSequence.Iterator(image))


def frame_durations(path: Path) -> list[int]:
    with Image.open(path) as image:
        return [frame.info.get("duration", 0) for frame in ImageSequence.Iterator(image)]


def write_manifest(skin: Path, clips: dict) -> None:
    (skin / "manifest.json").write_text(json.dumps({"clips": clips}), encoding="utf-8")


def run_tool(skin: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(skin)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def require_failure(skin: Path, expected_message: str) -> None:
    result = run_tool(skin)
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
            "talking.loop.right": valid_clip(),
        })

        stale = skin / "generated" / "clips" / "stale.gif"
        stale.parent.mkdir(parents=True, exist_ok=True)
        stale.write_bytes(b"stale")

        result = run_tool(skin)
        require(result.returncode == 0, result.stderr)

        output = skin / "generated" / "clips" / "talking.loop.right.gif"
        qrc = skin / "generated" / "clips.qrc"
        require(output.is_file(), "generated clip should be written")
        require(qrc.is_file(), "generated qrc should be written")
        require(not stale.exists(), "script should clean stale generated clips before writing")
        require(frame_count(output) == 3, "1-based inclusive [2, 4] should generate 3 frames")
        expected_durations = frame_durations(source)[1:4]
        require(
            frame_durations(output) == expected_durations,
            "generated clip should preserve source frame durations",
        )
        qrc_text = qrc.read_text(encoding="utf-8")
        require('<qresource prefix="/skins/test-skin/generated/clips">' in qrc_text, qrc_text)
        require('<file alias="talking.loop.right.gif">clips/talking.loop.right.gif</file>' in qrc_text, qrc_text)

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
