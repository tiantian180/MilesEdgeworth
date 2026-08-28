#!/usr/bin/env python3
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SKIN = ROOT / "apps/desktop/resources/skins/meow-market-leader"
EXPECTED_ACTIONS = {"watch", "strike", "seal", "spotlight", "fumble", "relax"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


skin = json.loads((SKIN / "skin.json").read_text(encoding="utf-8"))
manifest = json.loads((SKIN / "manifest.json").read_text(encoding="utf-8"))
cmake = (ROOT / "apps/desktop/CMakeLists.txt").read_text(encoding="utf-8")
event = (ROOT / "apps/desktop/src/pet/events/PetEvent.h").read_text(encoding="utf-8")
pipeline = (ROOT / "apps/desktop/src/pet/interaction/InteractionPipeline.cpp").read_text(encoding="utf-8")
surface = (ROOT / "apps/desktop/src/pet/surface/PetSurfaceWindow.cpp").read_text(encoding="utf-8")

require(skin["id"] == "meow-market-leader", "skin id must remain stable")
require(skin["version"] == "1.0.0", "bundled skin must use the released version")
require(skin["license"] == "GPL-3.0-or-later", "bundled skin must declare its distribution license")
require(skin["minAppVersion"] == "0.2.0", "bundled skin must declare its minimum app version")
require(skin["skinSchemaVersion"] == 1, "skin metadata must use schema v1")
require(manifest["schemaVersion"] == 4, "manifest must use schema v4")
require(set(manifest["actions"]) == EXPECTED_ACTIONS, "exactly six market actions are required")
require(len(manifest["skinCommands"]) == 6, "all six actions need native menu commands")
require(manifest["behaviorRules"] == [{"event": "pointer.followArrived", "action": "strike"}],
        "cursor-follow arrival must trigger the strike interaction")

for action_id, action in manifest["actions"].items():
    clip = action["variants"]["right"]["clip"]
    require(clip.startswith("file:"), f"{action_id}: clip must use a portable file URL")
    path = SKIN / clip.removeprefix("file:")
    require(path.is_file(), f"{action_id}: bundled GIF is missing")
    with Image.open(path) as image:
        require(image.format == "GIF", f"{action_id}: asset must decode as GIF")
        require(image.size == (512, 512), f"{action_id}: GIF canvas must be 512x512")
        require(image.n_frames >= 11, f"{action_id}: animation needs real pose progression")

thumbnail = SKIN / skin["thumbnail"].removeprefix("file:")
with Image.open(thumbnail) as image:
    require(image.size == (512, 512), "thumbnail must match the animation canvas")
    require(image.mode in {"RGBA", "LA"}, "thumbnail must preserve transparency")

for token in [
    "MEOW_MARKET_LEADER_SKIN_ROOT",
    "add_bundled_skin_copy",
    "skins/meow-market-leader",
]:
    require(token in cmake, f"CMake must distribute the market skin: missing {token}")

require("PointerFollowArrived" in event, "runtime must expose the cursor-follow arrival event")
require("pointer.followArrived" in pipeline, "interaction pipeline must resolve cursor-follow rules")
require("CursorFollowController" in surface, "desktop surface must own the cursor-follow controller")
require("CursorFollowSmoke" in cmake, "cursor-follow behavior needs a native smoke test")

skill = ROOT / "skills/desktop-pet-skin-factory"
for required in ["SKILL.md", "README.md", "VERSION", "scripts/compile_action.py", "scripts/verify_action.py"]:
    require((skill / required).is_file(), f"published skin factory is missing {required}")

print("meow market leader bundled skin contract ok")
