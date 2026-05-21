#!/usr/bin/env python3
"""检查静音开关会立即影响原生表面的语音音量。"""

from __future__ import annotations

import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def read_from_git(ref: str, path: str) -> str:
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    old_cpp = read_from_git("origin/legacy/v1-qt-widgets", "MilesEdgeworth.cpp")
    for token in [
        "actionMute",
        "soundEffect->setVolume(0)",
        "soundEffect->setVolume(0.8f)",
    ]:
        require(token in old_cpp, f"旧版静音逻辑缺少参考 token：{token}")

    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
    require("QSoundEffect" in surface_cpp, "PetSurfaceWindow 应使用 QSoundEffect 播放语音")
    require(
        "m_soundEffect->setVolume(m_runtime->audioMuted() ? 0.0f : 0.8f)" in surface_cpp,
        "QSoundEffect volume 应绑定 audioMuted，而不是固定 0.8",
    )
    require(
        "m_soundEffect->setVolume(0.8f)" not in surface_cpp,
        "静音后不能保留固定音量，否则正在播放的语音不会立即静音",
    )
    require(
        "muteAction->setChecked(runtime->audioMuted())" in menu_cpp
        and "PetRuntime::toggleAudioMuted" in menu_cpp,
        "静音菜单项应继续驱动 PetRuntime.audioMuted",
    )

    root_cmake = read("CMakeLists.txt")
    require("check_phase_0_50_mute_volume_binding" in root_cmake, "CTest 未注册 Phase 0.50 检查")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
