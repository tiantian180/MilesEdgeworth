#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIN_ROOT = ROOT / "apps/desktop/resources/skins/miles-edgeworth"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


skin_json_raw = (SKIN_ROOT / "skin.json").read_text(encoding="utf-8")
manifest_raw = (SKIN_ROOT / "manifest.json").read_text(encoding="utf-8")

json.loads(skin_json_raw)
json.loads(manifest_raw)

for token in ["skin:", "qrc:/pet/", "qrc:/audio/", "qrc:/skins/miles-edgeworth"]:
    require(token not in skin_json_raw, f"skin.json must not contain legacy URL token {token}")
    require(token not in manifest_raw, f"manifest.json must not contain legacy URL token {token}")

print("phase 1.4 skin url migration ok")
