# Phase 0 Desktop Shell Verification

本文档记录 v2 Qt/QML desktop shell 在 Phase 0 阶段的构建、运行、手动检查项和当前限制。目标是确认桌宠窗口壳层已经具备基本桌面行为，为后续 Pet Runtime、聊天窗口和设置界面接入做准备。

## Build

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

## Manual Checks

手动验证时，先完成对应架构的 Build，再使用 Run 命令启动应用。启动后检查以下桌面壳层行为：

- 透明无边框：桌宠窗口背景应透明，窗口本身不显示系统标题栏、边框或默认窗口装饰。
- 拖拽：按住桌宠可移动窗口位置，松开后窗口停留在新的桌面坐标。
- 失焦后可见：点击其他应用让桌宠失焦后，桌宠仍应保持可见，不应因为失焦而隐藏或最小化。
- 切换 Spaces 可见：在 macOS Spaces 之间切换时，桌宠应继续出现在当前 Space 中。
- 全屏 app 上方可见：切到全屏应用后，桌宠应仍显示在全屏应用上方。
- 动画资源 alias：确认 QML 使用的动画资源 alias 可以解析，`stand-right` 与 `stand-left` 能正常显示。

## Known Limits

- Phase 0 点击区域仍是矩形，尚未按透明像素或角色轮廓裁剪命中区域。
- Pet Runtime 未实现；当前只验证桌面窗口壳层和基础动画资源接入。
- 当前只注册 `stand-right` 和 `stand-left` 两个动画 alias。
- Windows/Linux 桌面层级、透明窗口、跨工作区和全屏覆盖行为需要另行验证。
