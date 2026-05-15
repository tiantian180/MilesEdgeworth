# MilesEdgeworth

MilesEdgeworth 正在从旧版 Qt Widgets 桌宠，升级为 Qt 6 / QML 驱动的 AI 桌宠应用。

当前 `v2-ai-pet` 分支只保留新版主线代码和可复用素材：

- `apps/desktop/`：Qt 6 / QML 桌面壳层。
- `docs/v2/`：新版架构、动画系统和阶段验证文档。
- `gifs/`：Miles 桌宠动画素材。
- `audios/`：旧版语音素材，后续可按皮肤 / 动作系统重新接入。
- `icon/`：图标素材。

旧版 Qt Widgets 源码、Visual Studio 工程、图片置顶查看器和旧版 macOS 打包脚本已经从该分支移除，避免干扰 v2 阅读和开发。需要参考旧版实现时，可以查看仓库历史或旧分支。

## 构建 v2 桌面壳层

macOS Apple Silicon + Homebrew 环境：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
open build/apps/desktop/MilesEdgeworthDesktop.app
```

也可以继续使用独立的外部 build 目录：

```bash
cmake -S apps/desktop -B /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build
open /Users/tian/projects/my-projects/MilesEdgeworth-v2-desktop-build/MilesEdgeworthDesktop.app
```

Intel Mac 的 Homebrew 默认路径通常是 `/usr/local`，对应把 `CMAKE_PREFIX_PATH` 改为 `/usr/local`。

## Cursor / clangd 代码提示

如果 C++ 代码里 `#include <QGuiApplication>`、`QWindow` 等 Qt 类型飘红，通常不是 Qt Extension Pack 没装好，而是 clangd 没读到 CMake 生成的 `compile_commands.json`。

先从仓库根目录配置一次 CMake：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
```

确认文件存在：

```bash
ls build/compile_commands.json
```

然后在 Cursor 命令面板执行：

```text
clangd: Restart language server
```

必要时再执行：

```text
CMake: Delete Cache and Reconfigure
```

## 当前状态

Phase 0 已完成最小桌面壳层验证：

- 透明无边框桌宠窗口。
- macOS 下跨 Spaces / 全屏应用置顶的技术验证。
- 左键拖拽。
- 右键显示最小退出菜单。
- 从资源系统加载 Miles GIF。

当前仍是技术验证，不是完整可发布的 v2 AI 桌宠。后续会继续实现 Pet Runtime、聊天窗口、设置中心、模型 Provider、皮肤 manifest 和 Agent Runtime。

## 文档

- [v2 架构设计](docs/v2/architecture.md)
- [Pet Runtime 动画系统设计](docs/v2/pet-runtime-animation-design.md)
- [Phase 0 桌面壳层验证](docs/v2/phase0-desktop-shell.md)

图片素材和音频素材来自游戏《逆转裁判》和《逆转检事》。本项目仅用于个人学习和技术验证。
