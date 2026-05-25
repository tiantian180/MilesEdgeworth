#!/usr/bin/env python3
"""检查随机 idle 候选池贴近旧版站立自动切换概率。"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"

DIRECT_FLIP_RECIPES = {"idle.flipStand"}
TURN_RECIPES = {"turn.once"}
ONCE_RECIPES = {
    "idle.randomThinking",
    "idle.tappingHead",
    "idle.shrug",
    "idle.checkWatch",
    "idle.pointing",
    "idle.sittingTea",
    "idle.phoneCall",
    "idle.lookBack",
    "idle.lookDown",
    "idle.lookUp",
}
WALK_RECIPES = {
    "walk.east",
    "walk.west",
    "walk.northEast",
    "walk.northWest",
    "walk.southEast",
    "walk.southWest",
    "walk.north",
    "walk.south",
}
RUN_RECIPES = {
    "run.east",
    "run.west",
    "run.northEast",
    "run.northWest",
    "run.southEast",
    "run.southWest",
    "run.north",
    "run.south",
}
SPECIAL_ONLY_RECIPES = {"bow.once", "objecting.once"}

EXPECTED_BUCKET_WEIGHTS = {
    "directFlip": 32,
    "turn": 48,
    "once": 240,
    "walk": 40,
    "run": 40,
}
EXPECTED_TOTAL_WEIGHT = sum(EXPECTED_BUCKET_WEIGHTS.values())


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def recipe_weights() -> dict[str, int]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    pool = manifest["animationPools"]["idle.random"]
    entries = pool["entries"]
    recipes = [entry["recipe"] for entry in entries]
    duplicates = sorted(recipe for recipe, count in Counter(recipes).items() if count > 1)

    require(not duplicates, f"idle.random 存在重复 recipe：{duplicates}")
    return {entry["recipe"]: int(entry.get("weight", 1)) for entry in entries}


def require_recipe_set(weights: dict[str, int]) -> None:
    expected = DIRECT_FLIP_RECIPES | TURN_RECIPES | ONCE_RECIPES | WALK_RECIPES | RUN_RECIPES
    actual = set(weights)

    require(
        expected <= actual,
        f"idle.random 缺少旧版站立随机候选：{sorted(expected - actual)}",
    )
    require(
        SPECIAL_ONLY_RECIPES.isdisjoint(actual),
        f"idle.random 混入旧版非随机待机特殊动作：{sorted(SPECIAL_ONLY_RECIPES & actual)}",
    )
    require(
        actual <= expected,
        f"idle.random 存在未归类候选，请先明确旧版语义：{sorted(actual - expected)}",
    )


def require_bucket_weights(weights: dict[str, int]) -> None:
    bucket_weights = {
        "directFlip": sum(weights[recipe] for recipe in DIRECT_FLIP_RECIPES),
        "turn": sum(weights[recipe] for recipe in TURN_RECIPES),
        "once": sum(weights[recipe] for recipe in ONCE_RECIPES),
        "walk": sum(weights[recipe] for recipe in WALK_RECIPES),
        "run": sum(weights[recipe] for recipe in RUN_RECIPES),
    }
    total_weight = sum(weights.values())

    require(
        total_weight == EXPECTED_TOTAL_WEIGHT,
        f"idle.random 总权重应为 {EXPECTED_TOTAL_WEIGHT}，当前为 {total_weight}",
    )
    require(
        bucket_weights == EXPECTED_BUCKET_WEIGHTS,
        f"idle.random 分桶权重应为 {EXPECTED_BUCKET_WEIGHTS}，当前为 {bucket_weights}",
    )

    for recipe in ONCE_RECIPES:
        require(weights[recipe] == 24, f"{recipe} 应保持一次性小动作等权重 24")
    for recipe in WALK_RECIPES | RUN_RECIPES:
        require(weights[recipe] == 5, f"{recipe} 应保持移动方向等权重 5")


def main() -> int:
    weights = recipe_weights()
    require_recipe_set(weights)
    require_bucket_weights(weights)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
