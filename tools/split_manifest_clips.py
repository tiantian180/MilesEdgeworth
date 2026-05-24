#!/usr/bin/env python3
import html
import json
import re
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image, ImageSequence
except ImportError as exc:
    raise SystemExit("Pillow is required. Run: python3 -m pip install -r tools/requirements.txt") from exc


CLIP_NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$")


def fail(message: str) -> None:
    raise SystemExit(message)


def require_object(value: object, message: str) -> dict:
    if not isinstance(value, dict):
        fail(message)
    return value


def resolve_skin_file(skin_root: Path, source: object, clip_name: str) -> Path:
    if not isinstance(source, str) or not source.startswith("file:"):
        fail(f"clip {clip_name}: source must use file: URL")

    relative = Path(source[len("file:"):])
    if relative.is_absolute():
        fail(f"clip {clip_name}: source escapes skin root")

    root = skin_root.resolve()
    resolved = (skin_root / relative).resolve()
    try:
        resolved.relative_to(root)
    except ValueError:
        fail(f"clip {clip_name}: source escapes skin root")
    return resolved


def parse_frame_range(value: object, clip_name: str) -> tuple[int, int]:
    if not isinstance(value, list) or len(value) != 2:
        fail(f"clip {clip_name}: frameRange must be [start, end]")
    start, end = value
    if not isinstance(start, int) or isinstance(start, bool):
        fail(f"clip {clip_name}: frameRange must contain integer frames")
    if not isinstance(end, int) or isinstance(end, bool):
        fail(f"clip {clip_name}: frameRange must contain integer frames")
    if start < 1 or end < start:
        fail(f"clip {clip_name}: frameRange must be 1-based inclusive")
    return start, end


def load_clip_frames(source_path: Path, start: int, end: int, clip_name: str) -> tuple[list[Image.Image], list[int]]:
    if not source_path.is_file():
        fail(f"clip {clip_name}: source file not found: {source_path}")

    with Image.open(source_path) as image:
        frames = []
        durations = []
        for index, frame in enumerate(ImageSequence.Iterator(image), start=1):
            if start <= index <= end:
                frames.append(frame.copy())
                durations.append(frame.info.get("duration", 0))

    expected = end - start + 1
    if len(frames) != expected:
        fail(f"clip {clip_name}: frameRange exceeds source frame count")
    return frames, durations


def write_clip(path: Path, frames: list[Image.Image], durations: list[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=0,
        disposal=2,
    )


def write_qrc(path: Path, skin_name: str, clip_names: list[str]) -> None:
    lines = [
        "<RCC>",
        f'  <qresource prefix="/skins/{html.escape(skin_name)}/generated/clips">',
    ]
    for clip_name in sorted(clip_names):
        escaped = html.escape(f"{clip_name}.gif")
        lines.append(f'    <file alias="{escaped}">clips/{escaped}</file>')
    lines.extend([
        "  </qresource>",
        "</RCC>",
        "",
    ])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def generate_clips(skin_root: Path) -> tuple[int, Path]:
    manifest_path = skin_root / "manifest.json"
    manifest = require_object(json.loads(manifest_path.read_text(encoding="utf-8")), "manifest must be an object")
    clips = require_object(manifest.get("clips"), "manifest must contain top-level clips object")

    prepared = []
    for clip_name, definition in clips.items():
        if not isinstance(clip_name, str) or not CLIP_NAME_RE.fullmatch(clip_name):
            fail(f"invalid clip name: {clip_name}")

        definition = require_object(definition, f"clip {clip_name}: definition must be an object")
        source_path = resolve_skin_file(skin_root, definition.get("source"), clip_name)
        start, end = parse_frame_range(definition.get("frameRange"), clip_name)
        frames, durations = load_clip_frames(source_path, start, end, clip_name)
        prepared.append((clip_name, frames, durations))

    generated_root = skin_root / "generated"
    clips_root = generated_root / "clips"
    if clips_root.exists():
        shutil.rmtree(clips_root)
    clips_root.mkdir(parents=True, exist_ok=True)

    for clip_name, frames, durations in prepared:
        write_clip(clips_root / f"{clip_name}.gif", frames, durations)

    qrc_path = generated_root / "clips.qrc"
    write_qrc(qrc_path, skin_root.name, [clip_name for clip_name, _, _ in prepared])
    return len(prepared), qrc_path


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: split_manifest_clips.py <skin-root>")

    skin_root = Path(argv[1])
    count, qrc_path = generate_clips(skin_root)
    print(f"generated {count} clips")
    print(qrc_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
