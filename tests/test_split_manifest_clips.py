#!/usr/bin/env python3
import json
import shutil
import subprocess
import sys
from pathlib import Path

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


def main() -> None:
    if TMP.exists():
        shutil.rmtree(TMP)
    skin = TMP / "test-skin"
    source = skin / "assets" / "body" / "raw" / "source.gif"
    make_source_gif(source)

    (skin / "manifest.json").write_text(json.dumps({
        "clips": {
            "talking.loop.right": {
                "source": "file:assets/body/raw/source.gif",
                "frameRange": [2, 4],
            }
        }
    }), encoding="utf-8")

    stale = skin / "generated" / "clips" / "stale.gif"
    stale.parent.mkdir(parents=True, exist_ok=True)
    stale.write_bytes(b"stale")

    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(skin)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
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

    bad_manifest = skin / "manifest.json"
    bad_manifest.write_text(json.dumps({
        "clips": {
            "../escape": {
                "source": "file:assets/body/raw/source.gif",
                "frameRange": [1, 1],
            }
        }
    }), encoding="utf-8")
    bad = subprocess.run(
        [sys.executable, str(SCRIPT), str(skin)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    require(bad.returncode != 0, "unsafe clip name should fail")
    require("invalid clip name" in bad.stderr, bad.stderr)

    shutil.rmtree(TMP)
    print("phase 2.4.2 clip slicing tool ok")


if __name__ == "__main__":
    main()
