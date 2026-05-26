#!/usr/bin/env python3
from __future__ import annotations

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


def load_manifest(manifest_path: Path) -> dict:
    if not manifest_path.is_file():
        fail("manifest not found")

    try:
        return require_object(json.loads(manifest_path.read_text(encoding="utf-8")), "manifest must be an object")
    except json.JSONDecodeError:
        fail("manifest is invalid JSON")
    except UnicodeDecodeError:
        fail("manifest is invalid JSON")


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

    try:
        with Image.open(source_path) as image:
            if image.format != "GIF":
                fail(f"clip {clip_name}: unable to read source GIF: {source_path}")

            frames = []
            durations = []
            for index, frame in enumerate(ImageSequence.Iterator(image), start=1):
                if start <= index <= end:
                    frames.append(frame.copy().convert("RGBA"))
                    durations.append(frame.info.get("duration", 0))
    except OSError:
        fail(f"clip {clip_name}: unable to read source GIF: {source_path}")

    expected = end - start + 1
    if len(frames) != expected:
        fail(f"clip {clip_name}: frameRange exceeds source frame count")
    return frames, durations


def images_equal(left: Image.Image, right: Image.Image) -> bool:
    return left.mode == right.mode and left.size == right.size and left.tobytes() == right.tobytes()


def transparent_pixel(image: Image.Image) -> tuple[int, int] | None:
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            if pixels[x, y][3] == 0:
                return x, y
    return None


def preserve_duplicate_frames(frames: list[Image.Image], clip_name: str) -> list[Image.Image]:
    prepared = []
    previous = None
    for index, frame in enumerate(frames):
        image = frame.copy()
        is_duplicate = previous is not None and images_equal(image, previous)
        sentinel = transparent_pixel(image)
        if sentinel is None and is_duplicate:
            fail(f"clip {clip_name}: duplicate opaque frames cannot be preserved without changing pixels")
        if sentinel is not None:
            # GIF encoders may merge identical adjacent frames. Vary RGB under a fully
            # transparent pixel so the encoded frames differ without changing output.
            image.putpixel(sentinel, ((index * 73) % 256, (index * 151) % 256, (index * 199) % 256, 0))
        prepared.append(image)
        previous = frame
    return prepared


def generated_durations(path: Path) -> list[int]:
    with Image.open(path) as image:
        return [frame.info.get("duration", 0) for frame in ImageSequence.Iterator(image)]


def write_clip(path: Path, clip_name: str, frames: list[Image.Image], durations: list[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    prepared = preserve_duplicate_frames(frames, clip_name)
    prepared[0].save(
        path,
        save_all=True,
        append_images=prepared[1:],
        duration=durations,
        loop=0,
        disposal=2,
        optimize=False,
    )
    if generated_durations(path) != durations:
        fail(f"clip {clip_name}: generated GIF did not preserve frame count or durations")


def generate_clips(skin_root: Path) -> tuple[Path, int]:
    manifest_path = skin_root / "manifest.json"
    manifest = load_manifest(manifest_path)
    clips = require_object(manifest.get("clips"), "manifest.json must contain object field: clips")

    prepared = []
    for clip_name, definition in sorted(clips.items()):
        if not isinstance(clip_name, str) or not CLIP_NAME_RE.fullmatch(clip_name):
            fail(f"invalid clip name: {clip_name}")

        definition = require_object(definition, f"clip {clip_name}: definition must be an object")
        source_path = resolve_skin_file(skin_root, definition.get("source"), clip_name)
        start, end = parse_frame_range(definition.get("frameRange"), clip_name)
        frames, durations = load_clip_frames(source_path, start, end, clip_name)
        prepared.append((clip_name, frames, durations))

    generated_parent = skin_root / "generated"
    generated_root = generated_parent / "clips"
    temp_root = generated_parent / ".clips.tmp"
    if temp_root.exists():
        shutil.rmtree(temp_root)
    temp_root.mkdir(parents=True, exist_ok=True)

    try:
        for clip_name, frames, durations in prepared:
            write_clip(temp_root / f"{clip_name}.gif", clip_name, frames, durations)
    except BaseException:
        if temp_root.exists():
            shutil.rmtree(temp_root)
        raise

    if generated_root.exists():
        shutil.rmtree(generated_root)
    temp_root.replace(generated_root)

    legacy_qrc = generated_parent / "clips.qrc"
    if legacy_qrc.is_file():
        legacy_qrc.unlink()

    return generated_root, len(prepared)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: split_manifest_clips.py <skin-root>")

    clips_root, count = generate_clips(Path(argv[1]).resolve())
    print(f"generated {count} clips under {clips_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
