#!/usr/bin/env python3
"""检查 walk/run 完成后的续行动作权重贴近旧版。"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"

WALK_DIRECTIONS = {
    "walk.east",
    "walk.west",
    "walk.northEast",
    "walk.northWest",
    "walk.southEast",
    "walk.southWest",
    "walk.north",
    "walk.south",
}
RUN_DIRECTIONS = {
    "run.east",
    "run.west",
    "run.northEast",
    "run.northWest",
    "run.southEast",
    "run.southWest",
    "run.north",
    "run.south",
}

EXPECTED_WALK_BUCKETS = {
    "continueWalk": 400,
    "switchToRun": 80,
    "randomWalk": 120,
    "stand": 200,
}
EXPECTED_RUN_BUCKETS = {
    "continueRun": 400,
    "switchToWalk": 80,
    "randomRun": 80,
    "stand": 240,
}
EXPECTED_TOTAL_WEIGHT = 800


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def action_pool_weights(pool_id: str) -> dict[str, int]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = manifest["animationPools"][pool_id]["entries"]
    recipes = [entry["recipe"] for entry in entries]
    duplicates = sorted(recipe for recipe, count in Counter(recipes).items() if count > 1)

    require(not duplicates, f"{pool_id} 存在重复 recipe：{duplicates}")
    return {entry["recipe"]: int(entry.get("weight", 1)) for entry in entries}


def require_walk_finished(weights: dict[str, int]) -> None:
    expected_recipes = {"walk.current", "run.current", "idle.stand"} | WALK_DIRECTIONS
    actual_recipes = set(weights)
    require(actual_recipes == expected_recipes, f"walk.finished 候选应为 {sorted(expected_recipes)}，当前为 {sorted(actual_recipes)}")

    buckets = {
        "continueWalk": weights["walk.current"],
        "switchToRun": weights["run.current"],
        "randomWalk": sum(weights[recipe] for recipe in WALK_DIRECTIONS),
        "stand": weights["idle.stand"],
    }

    require(sum(weights.values()) == EXPECTED_TOTAL_WEIGHT, f"walk.finished 总权重应为 {EXPECTED_TOTAL_WEIGHT}")
    require(buckets == EXPECTED_WALK_BUCKETS, f"walk.finished 分桶权重应为 {EXPECTED_WALK_BUCKETS}，当前为 {buckets}")
    for recipe in WALK_DIRECTIONS:
        require(weights[recipe] == 15, f"{recipe} 应保持随机走路方向等权重 15")


def require_run_finished(weights: dict[str, int]) -> None:
    expected_recipes = {"run.current", "walk.current", "idle.stand"} | RUN_DIRECTIONS
    actual_recipes = set(weights)
    require(actual_recipes == expected_recipes, f"run.finished 候选应为 {sorted(expected_recipes)}，当前为 {sorted(actual_recipes)}")

    buckets = {
        "continueRun": weights["run.current"],
        "switchToWalk": weights["walk.current"],
        "randomRun": sum(weights[recipe] for recipe in RUN_DIRECTIONS),
        "stand": weights["idle.stand"],
    }

    require(sum(weights.values()) == EXPECTED_TOTAL_WEIGHT, f"run.finished 总权重应为 {EXPECTED_TOTAL_WEIGHT}")
    require(buckets == EXPECTED_RUN_BUCKETS, f"run.finished 分桶权重应为 {EXPECTED_RUN_BUCKETS}，当前为 {buckets}")
    for recipe in RUN_DIRECTIONS:
        require(weights[recipe] == 10, f"{recipe} 应保持随机跑步方向等权重 10")


def main() -> int:
    require_walk_finished(action_pool_weights("walk.finished"))
    require_run_finished(action_pool_weights("run.finished"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
