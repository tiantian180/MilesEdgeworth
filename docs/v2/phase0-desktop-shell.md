# Phase 0 Desktop Shell Verification

本文档记录 v2 Qt/QML desktop shell 在 Phase 0 阶段的构建、运行、手动检查项和当前限制。目标是确认桌宠窗口壳层已经具备基本桌面行为，为后续 Pet Runtime、聊天窗口和设置界面接入做准备。

## Build

### 前置工具

Phase 0 构建需要先安装 Xcode Command Line Tools、CMake、Ninja 和 Qt 6。当前验证环境是 Qt 6.11；工程基础要求保持在 Qt 6.5+，并对 Qt 6.8+ 的 QML policy 做了兼容处理。Apple Silicon Mac 通常使用 Homebrew 的 `/opt/homebrew` 前缀；Intel Mac 通常使用 `/usr/local` 前缀。

```sh
xcode-select --install
brew install cmake ninja qt
```

安装后可以用这些命令确认工具可用：

```sh
clang++ --version
cmake --version
ninja --version
qtpaths6 --version
```

### Apple Silicon

```sh
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

### Intel Mac

```sh
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/usr/local
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
```

## Run

```sh
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

## Automated Checks

窗口层级仍然需要真实 macOS 桌面环境手动验证，但仓库里保留了一个小的静态检查，
用来防止普通“始终置顶”开关重新依赖 SkyLight 私有 Space：

```sh
python3 tests/check_macos_window_behavior.py
```

## Manual Checks

手动验证时，先完成对应架构的 Build，再使用 Run 命令启动应用。启动后检查以下桌面壳层行为：

- 透明无边框：桌宠窗口背景应透明，窗口本身不显示系统标题栏、边框或默认窗口装饰。
- 拖拽：按住桌宠可移动窗口位置，松开后窗口停留在新的桌面坐标。
- 退出入口：右键桌宠应弹出最小菜单，点击“退出”后应用关闭。
- 置顶开关：右键桌宠应能在“取消置顶”和“始终置顶”之间切换。
- 失焦后可见：点击其他应用让桌宠失焦后，桌宠仍应保持可见，不应因为失焦而隐藏或最小化。
- 切换 Spaces 可见：在 macOS Spaces 之间切换时，桌宠应继续出现在当前 Space 中。
- 全屏 app 上方可见：切到全屏应用后，桌宠应仍显示在全屏应用上方。
- 取消置顶后可覆盖：点击“取消置顶”后，桌宠仍不应因为应用失焦而隐藏，但允许被其他窗口覆盖。重新点击“始终置顶”后，应恢复当前 Phase 0 的跨 Space / 全屏上方显示行为。
- 动画资源 alias：运行态确认 `qrc:/pet/stand-right.gif` 可以解析并显示。`stand-left` 已注册 alias，但 Phase 0 还没有切换朝向的运行时入口，后续由 Pet Runtime 验证。

## Known Limits

- Phase 0 点击区域仍是矩形，尚未按透明像素或角色轮廓裁剪命中区域。
- Phase 0 只有最小右键菜单，包含置顶开关和退出入口；尚未实现旧版完整右键菜单或系统托盘菜单。
- Pet Runtime 未实现；当前只验证桌面窗口壳层和基础动画资源接入。
- 当前只注册 `stand-right` 和 `stand-left` 两个动画 alias。
- macOS 普通置顶开关使用标准 AppKit 窗口层级：置顶时切到 screen saver level 并加入所有 Space，取消置顶时降回普通窗口层级。这样比 SkyLight 私有 Space 更适合做可逆开关。
- SkyLight 私有 Space 暂不作为普通置顶开关的实现。它可以作为未来“固定在屏幕最上层”的实验模式单独设计，但需要接受私有 API、不稳定、退出时可能要重建窗口等代价。
- macOS 全屏 / Mission Control 场景仍需要真实机器手动验证。Qt 的 QWindow 不是原生 NSPanel；如果后续发现全屏覆盖能力不足，下一步应改成 macOS 专用 NSPanel 容器，而不是把普通置顶开关重新绑到 SkyLight。
- Phase 0 暂时不使用 `Qt.Tool`。macOS 由 `NSApplicationActivationPolicyAccessory` 和原生窗口属性承担辅助应用行为；Windows/Linux 的任务栏隐藏策略后续再单独验证。
- Windows/Linux 桌面层级、透明窗口、跨工作区和全屏覆盖行为需要另行验证。
