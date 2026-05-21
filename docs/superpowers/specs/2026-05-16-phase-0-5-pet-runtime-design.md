# Phase 0.5 Pet Runtime 最小纵切设计

## 背景

Phase 0 已经完成 Qt/QML 桌宠壳层的基础验证：透明无边框窗口、拖拽、平台右键菜单、macOS 可逆置顶开关和基础 GIF 资源接入。当前 `PetWindow.qml` 仍直接写死 `qrc:/pet/stand-right.gif`，`DesktopShellController` 也通过 `setContextProperty()` 注入 QML，导致 QML 静态工具无法理解 `desktopShell`。

Phase 0.5 的目标是把“窗口能显示一个 GIF”推进为“有最小动作运行时的桌宠”。这一阶段仍不接入大模型、聊天窗口、完整皮肤系统或移动路径规划，只打通后续 AI 事件驱动动画所需的最短链路。

## 目标

本阶段交付：

- `DesktopShellController` 以 QML 可识别的单例方式暴露，替代 context property。
- 新增 `PetRuntime`，作为 Qt 侧桌宠动作状态机的最小入口。
- 新增一个最小 skin manifest，声明 `idle`、`thinking`、`speaking` 三个动作及其动画资源。
- QML 不再直接写死站立 GIF，而是绑定 `PetRuntime.currentAnimationUrl`。
- 右键菜单新增开发测试入口：回到待机、测试思考、测试说话。
- 保留现有桌面壳层能力，不改变窗口透明、拖拽、置顶、退出行为。
- 文档更新 Phase 0.5 的边界、测试方式和后续扩展方向。

非目标：

- 不实现完整 Animation Orchestrator。
- 不实现 enter/loop/exit 阶段动画。
- 不实现移动驱动动画、随机 idle、点击/双击复杂交互。
- 不接入 Go sidecar、模型 API、聊天窗口或 Dashboard。
- 不把所有旧版 GIF 全部命名整理。

## 推荐方案

采用 C++ `QObject` 控制器 + Qt QML foreign singleton 类型注册：

- `DesktopShellController` 和 `PetRuntime` 都作为 C++ 对象存在，由 `main.cpp` 创建，生命周期长于 `QQmlApplicationEngine`。
- 使用 `QML_FOREIGN` + `QML_SINGLETON` + `QML_NAMED_ELEMENT` 声明单例类型，让 QML 通过 `DesktopShell`、`PetRuntime` 名称访问已有 C++ 实例。
- 将头文件放入 `qt_add_qml_module()` 的 `SOURCES`，让 Qt 生成 QML 类型信息，改善 QML Language Server 的补全和 lint 行为。

这样比继续使用 `setContextProperty()` 更适合后续扩展，也比只调用 `qmlRegisterSingletonInstance()` 更容易让 `qmllint` 和编辑器理解类型。

## 模块边界

### DesktopShellController

职责保持不变：

- 管理桌宠窗口对象。
- 暴露 `alwaysOnTop`。
- 响应右键菜单的置顶切换。
- 调用平台层窗口行为。

QML 访问方式从：

```qml
desktopShell.alwaysOnTop
desktopShell.toggleAlwaysOnTop()
```

改为：

```qml
App.DesktopShell.alwaysOnTop
App.DesktopShell.toggleAlwaysOnTop()
```

`PetWindow.qml` 使用 `import MilesEdgeworth as App`，显式标明这些对象来自本项目 QML module，避免 `qmllint` 报 unqualified access。

### PetRuntime

新增最小职责：

- 暴露 `currentState`。
- 暴露 `currentActionId`。
- 暴露 `currentAnimationUrl`。
- 提供 `setState(state)`、`playAction(actionId)`、`returnToIdle()`、`testThinking()`、`testSpeaking()`。
- 从内置 skin manifest 加载动作到动画资源的映射。

PetRuntime 只选择“现在播放哪条动画”，不负责窗口移动、透明点击区域、气泡、音效或 badge。

### Skin Manifest

Phase 0.5 使用一个最小 JSON 文件：

```json
{
  "id": "miles-edgeworth",
  "name": "Miles Edgeworth",
  "version": 1,
  "defaultFacing": "right",
  "states": {
    "idle": { "action": "idle_stand" },
    "thinking": { "action": "thinking" },
    "speaking": { "action": "objecting" }
  },
  "actions": {
    "idle_stand": {
      "animation": "qrc:/pet/stand-right.gif",
      "loop": true
    },
    "thinking": {
      "animation": "qrc:/pet/thinking-right.gif",
      "loop": true
    },
    "objecting": {
      "animation": "qrc:/pet/objecting-right.gif",
      "loop": false
    }
  },
  "fallbackAction": "idle_stand"
}
```

本阶段 manifest 先作为资源文件打进 Qt qrc。后续可以迁移到 `assets/skins/miles-edgeworth/manifest.json`，并支持用户导入皮肤。

## 资源映射

本阶段只注册少量旧素材：

- `stand-right.gif`：沿用 `gifs/stand/0.gif`。
- `stand-left.gif`：沿用 `gifs/stand/1.gif`，暂不接入朝向切换。
- `thinking-right.gif`：使用 `gifs/once/4.gif`，对应“抱胸思考”动作。
- `objecting-right.gif`：使用 `gifs/special/object0.gif`，对应旧版特殊动作里的“异议/强调”资源。

如果某个动作资源后续确认语义不准确，只改 manifest/qrc 映射，不改 PetRuntime 接口。

## QML 行为

`PetWindow.qml` 只做表现层绑定：

- `AnimatedImage.source: App.PetRuntime.currentAnimationUrl`
- 菜单项读取 `App.DesktopShell.alwaysOnTop`
- 开发菜单调用 `App.PetRuntime.returnToIdle()`、`App.PetRuntime.testThinking()`、`App.PetRuntime.testSpeaking()`

QML 不解析 manifest，不根据状态选择具体 GIF。

## 错误处理

- manifest 读取失败时，PetRuntime 使用内置 fallback：`qrc:/pet/stand-right.gif`。
- 未知 state 降级到 `idle`。
- 未知 action 降级到 `fallbackAction`。
- 空动画 URL 不传给 QML；PetRuntime 必须始终提供可播放 URL。

## 测试策略

新增静态/轻量检查脚本，覆盖：

- `PetWindow.qml` 不再直接包含 `qrc:/pet/stand-right.gif`。
- QML 使用 `DesktopShell` 和 `PetRuntime`，不再使用 `desktopShell` context property。
- `main.cpp` 不再调用 `setContextProperty("desktopShell", ...)`。
- `main.cpp` 注册 `DesktopShell` 和 `PetRuntime` 单例。
- skin manifest 包含 `idle`、`thinking`、`speaking` 三个状态。

继续保留：

- `ctest --test-dir build --output-on-failure`
- `qmllint apps/desktop/qml/PetWindow.qml`
- 本地手动运行桌宠，验证菜单项能切换动画。

## 后续扩展

Phase 0.5 完成后，下一步可以进入：

- Phase 0.6：整理更多旧 GIF 动作名和左右朝向。
- Phase 1：聊天窗口和基础模型 API。
- Phase 1.5：模型流式事件驱动 `thinking` / `speaking` / `idle`。
- Phase 2：expression tag 到 action 的动态映射。
