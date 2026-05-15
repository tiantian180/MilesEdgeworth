#!/usr/bin/env python3
"""检查 macOS 桌宠窗口策略是否保持在可逆实现上。

这个脚本不是替代手动 UI 验证的端到端测试。它只负责守住一个很关键的
架构边界：普通“始终置顶”开关不能再把窗口移动到 SkyLight 私有 Space。
那种做法适合实验性的强覆盖层，但不适合作为用户可以随时取消的普通开关。
"""

from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
MAC_BEHAVIOR = ROOT / "apps/desktop/src/platform/MacPetWindowBehavior.mm"


def extract_function(source: str, name: str) -> str:
    """用简单括号计数提取函数体，避免为了一个静态检查引入额外依赖。"""
    match = re.search(rf"\b{name}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"找不到函数：{name}")

    start = match.start()
    index = match.end() - 1
    depth = 0
    while index < len(source):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
        index += 1

    raise AssertionError(f"函数括号没有闭合：{name}")


def main() -> int:
    source = MAC_BEHAVIOR.read_text(encoding="utf-8")
    function_body = extract_function(source, "setMacPetWindowAlwaysOnTop")

    forbidden_tokens = [
        "moveWindowToStationarySkyLightSpace",
        "stationarySkyLightSpace",
        "SLSSpaceAddWindowsAndRemoveFromSpaces",
        "kCGAssistiveTechHighWindowLevelKey",
    ]

    violations = [token for token in forbidden_tokens if token in function_body]
    if violations:
        joined = ", ".join(violations)
        print(f"普通置顶开关仍然依赖不可逆的 SkyLight 私有 Space 路径：{joined}", file=sys.stderr)
        return 1

    required_tokens = [
        "kCGScreenSaverWindowLevelKey",
        "NSWindowCollectionBehaviorCanJoinAllSpaces",
        "NSWindowCollectionBehaviorFullScreenAuxiliary",
        "NSNormalWindowLevel",
        "NSWindowCollectionBehaviorMoveToActiveSpace",
    ]
    missing = [token for token in required_tokens if token not in function_body]
    if missing:
        joined = ", ".join(missing)
        print(f"普通置顶开关缺少预期的可逆 AppKit 行为：{joined}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
