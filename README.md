# MilesEdgeworth

MilesEdgeworth 正在从旧版 Qt Widgets 桌宠，升级为 Qt 6 / QML 驱动的 AI 桌宠应用。

当前 `v2-ai-pet` 分支只保留新版主线代码和可复用素材：

- `apps/desktop/`：Qt 6 / QML 桌面壳层。
- `apps/desktop/resources/skins/miles-edgeworth/assets/`：Miles 皮肤包资源，包括动画、语音、Prop 图片和源素材。
- `docs/v2/`：新版文档，按设计方案、参考资料、阶段记录分类。
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
- 系统托盘图标和托盘退出入口。
- 旧版左下角公文包入场启动位置。
- 旧版身体点位移动边界，允许透明留白略微越界。
- 旧版“双屏选项”菜单和双屏横向移动边界。
- 鼠标移入桌宠时显示旧版手型光标。
- 旧版 clickTimer 风格的单双击判定，避免双击后补触发单击反应。
- 旧版静音音量语义，静音时正在播放的语音也会立即降到 0。
- 站立循环后的随机待机触发概率已迁入皮肤 manifest，不再写死在运行时。
- Miles GIF、语音和检察官徽章图片已迁入皮肤包 `assets/`，运行时继续通过稳定 qrc alias 加载。
- `PetRuntime` 已开始拆分：manifest 数据结构、JSON 加载、候选池选择和 behavior trigger 选择已移出单体运行时。

当前仍是技术验证，不是完整可发布的 v2 AI 桌宠。后续会继续实现 Pet Runtime、聊天窗口、设置中心、模型 Provider、皮肤 manifest 和 Agent Runtime。

## 文档

- [v2 文档索引](docs/v2/文档索引.md)
- [总体架构设计](docs/v2/设计方案/总体架构设计.md)
- [桌宠运行时与动画调度设计](docs/v2/设计方案/桌宠运行时与动画调度设计.md)
- [皮肤包播放行为设计](docs/v2/设计方案/皮肤包播放行为设计.md)
- [动画素材盘点](docs/v2/参考资料/动画素材盘点.md)
- [第0阶段桌面壳验证](docs/v2/阶段记录/第0阶段桌面壳验证.md)

图片素材和音频素材来自游戏《逆转裁判》和《逆转检事》。本项目仅用于个人学习和技术验证。
