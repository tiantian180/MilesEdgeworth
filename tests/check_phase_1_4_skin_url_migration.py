#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
qrc_path = ROOT / "apps/desktop/resources/pet_assets.qrc"
raw = manifest_path.read_text(encoding="utf-8")

if "qrc:/pet/" in raw or "qrc:/audio/" in raw:
    raise AssertionError("built-in Miles manifest should use skin: URLs, not qrc:/pet or qrc:/audio")

manifest = json.loads(raw)


def walk(value):
    if isinstance(value, dict):
        for item in value.values():
            yield from walk(item)
    elif isinstance(value, list):
        for item in value:
            yield from walk(item)
    elif isinstance(value, str):
        yield value


skin_urls = [value for value in walk(manifest) if value.startswith("skin:")]
if not skin_urls:
    raise AssertionError("manifest should contain skin: URLs")

required = [
    "skin:assets/body/idle/stand-right.gif",
    "skin:assets/body/locomotion/walk-east.gif",
    "skin:assets/body/locomotion/run-east.gif",
    "skin:assets/props/prosecutor_badge/prosecutor-badge.png",
    "skin:assets/audio/voice/holdit0.wav",
]
missing = [url for url in required if url not in skin_urls]
if missing:
    raise AssertionError(f"missing required skin URLs: {missing}")

qrc_text = qrc_path.read_text(encoding="utf-8")
skin_section = re.search(
    r'<qresource\s+prefix="/skins/miles-edgeworth">(.*?)</qresource>',
    qrc_text,
    re.S,
)
if skin_section is None:
    raise AssertionError("qrc must expose /skins/miles-edgeworth resource block")

aliases = set(re.findall(r'alias="([^"]+)"', skin_section.group(1)))

unknown = []
for url in skin_urls:
    relative = url[len("skin:"):].lstrip("/")
    if relative not in aliases:
        unknown.append(url)

if unknown:
    raise AssertionError(
        "skin: URLs not present in /skins/miles-edgeworth qrc aliases:\n  "
        + "\n  ".join(sorted(unknown))
    )

print("phase 1.4 skin url migration ok")
