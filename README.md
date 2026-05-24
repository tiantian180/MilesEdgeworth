# MilesEdgeworth

MilesEdgeworth 正在从旧版 Qt Widgets 桌宠，升级为 Qt 6 驱动的可换肤 AI 桌宠应用。

当前 `v2-ai-pet` 分支处在 v2 框架建设阶段。桌宠本体已经从 Qt Quick/QML Window 切换为原生 `QWidget` / `QMovie` surface，用逐帧 alpha mask 解决透明像素拦截鼠标的问题；后续聊天窗口、设置中心和开发者工具仍可以继续使用 QML 或其他更适合的 UI 技术。

当前 `v2-ai-pet` 分支只保留新版主线代码和可复用素材：

- `apps/desktop/`：Qt 6 桌面壳层、Pet Runtime、原生桌宠 surface、系统菜单和平台窗口适配。
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

常用验证命令：

```bash
python3 -m pip install -r tools/requirements.txt
python3 tests/test_split_manifest_clips.py
python3 tests/check_phase_2_4_2_precut_clips.py
ctest --test-dir build --output-on-failure
git diff --check
```

当前桌宠本体不再加载 `PetWindow.qml` 作为主窗口，所以日常改 C++ / manifest / 文档时不把 `qmllint` 作为必跑门禁。后续新增 QML 聊天窗口或设置窗口时，再为对应 QML 文件恢复专门 lint。

## 皮肤包加载

v2 支持从文件系统加载皮肤包。皮肤包是一个普通目录，至少包含：

```text
my-skin/
  skin.json
  manifest.json
  assets/
```

右键桌宠 → `皮肤` → `打开皮肤目录` 可以打开当前用户皮肤目录。把皮肤目录放进去后，选择 `重载当前皮肤` 或重启应用即可重新扫描。

内置 Miles 皮肤仍打包在应用内；文件系统里出现同 id 皮肤时，用户皮肤优先。

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

Phase 1 框架主干和收尾体验修复已经完成，当前主线进入 Phase 2：AI 聊天桌宠最小闭环。

已完成的主干能力：

- 透明无边框桌宠窗口。
- macOS 下跨 Spaces / 全屏应用置顶的技术验证。
- 原生 `PetSurfaceWindow` / `PropSurfaceWindow`，使用当前动画帧 alpha mask 让透明像素尽量鼠标穿透。
- 左键拖拽。
- 系统原生右键菜单。
- 系统托盘图标和托盘退出入口。
- 旧版左下角公文包入场启动位置。
- 旧版身体点位移动边界，允许透明留白略微越界。
- 旧版“双屏选项”菜单和双屏横向移动边界。
- 鼠标移入桌宠时显示旧版手型光标。
- 旧版 clickTimer 风格的单双击判定，避免双击后补触发单击反应。
- 旧版静音音量语义，静音时正在播放的语音也会立即降到 0。
- 站立循环后的随机待机触发概率已迁入皮肤 manifest，不再写死在运行时。
- Miles GIF、语音和检察官徽章图片已迁入皮肤包 `assets/`；manifest 已升级到 schema v4，资源使用 `file:assets/...`，thinking/talking 局部循环通过顶层 `clips` 在构建期预切片到 `generated/clips/`，运行时只播放完整 GIF。
- 右键菜单已移除开发测试入口；Miles 红茶从皮肤定制命令进入，睡觉/唤醒继续作为通用 sleep/rest 能力保留。
- `PetRuntime` 已开始拆分：manifest 数据结构、JSON 加载、候选池选择、behavior trigger 选择、单击 hit zone 命中逻辑、皮肤命令解析、Prop 状态管理，以及 QML 事件到 ActionRequest 的主干已移出单体运行时。
- Custom Interaction Host API 已接入，Miles 旧版双击概率“看招”丢检察官徽章已作为皮肤侧高级交互回归。
- 语音语言已改为可选 Audio Capability，Miles 皮肤通过 manifest 声明日语、英语、中文，右键菜单按声明动态生成语音子菜单。
- ExpressionMapping schema 已接入，后续 AI / Agent 可以请求当前皮肤声明的 expression，由运行时映射到具体动作。
- Phase 2.4 聊天动画编排已接入：thinking enter/loop/exit、onceThenHold 定帧动作、segment queue 双条件门控、lifecycle SSE 事件和 runtime-controlled recipe step。

Phase 2 期间可以并行处理的框架债务（统一跟踪见 [技术债务与评审待办](docs/v2/参考资料/技术债务与评审待办.md#readme-同步的框架债务)）：

- HitZone schema 迁到 image-space。
- 连续缩放控件。
- Pet Skin Studio。
- `PetRuntime` 内部继续拆 `PlaybackController` / `RecipeRunner`。

当前仍是技术验证，不是完整可发布的 v2 AI 桌宠。后续会继续实现 Pet Runtime、聊天窗口、设置中心、模型 Provider、皮肤 manifest 和 Agent Runtime。

## 文档

- [v2 文档索引](docs/v2/文档索引.md)
- [总体架构设计](docs/v2/设计方案/总体架构设计.md)
- [桌宠运行时与动画调度设计](docs/v2/设计方案/桌宠运行时与动画调度设计.md)
- [桌宠运行时职责拆分设计](docs/v2/设计方案/桌宠运行时职责拆分设计.md)
- [皮肤包播放行为设计](docs/v2/设计方案/皮肤包播放行为设计.md)
- [皮肤包分发与加载机制设计](docs/v2/设计方案/皮肤包分发与加载机制设计.md)
- [HitZone 交互区域系统设计](docs/v2/设计方案/HitZone%20交互区域系统设计.md)
- [动画素材盘点](docs/v2/参考资料/动画素材盘点.md)
- [第0阶段桌面壳验证](docs/v2/阶段记录/第0阶段桌面壳验证.md)
- [Phase 0.65-0.73 工作报告](docs/v2/阶段记录/Phase%200.65-0.73%20工作报告.md)
- [Phase 1 收尾与体验问题修复计划](docs/v2/阶段记录/Phase%201%20收尾与体验问题修复计划.md)
- [Phase 2 AI 聊天粗规划](docs/v2/阶段记录/Phase%202%20AI%20聊天粗规划.md)
- [Phase 2.4 Phased 动画与动画链](docs/v2/阶段记录/Phase%202.4%20Phased%20动画与动画链.md)

图片素材和音频素材来自游戏《逆转裁判》和《逆转检事》。本项目仅用于个人学习和技术验证。
