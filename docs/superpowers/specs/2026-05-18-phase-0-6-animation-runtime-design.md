# Phase 0.6 Animation Runtime Skeleton Design

## 背景

Phase 0.5 已经把 `DesktopShell` 和 `PetRuntime` 作为 QML singleton 暴露，并用一个最小 manifest 跑通了 `idle`、`thinking`、`speaking` 三个状态。当前问题是：manifest 还不能表达左右朝向、动作是否循环、一次性动作播完后怎么回到 idle，也没有一份面向后续整理素材的资源盘点文档。

Phase 0.6 不做完整动画编排器，只建立下一层骨架，让后续接随机 idle、移动驱动动画、点击触发动画和模型动作标签时，不必重写 `PetRuntime` 的基础接口。

## 目标

- 给旧 GIF 建一份初始资源盘点文档，记录已知语义、朝向规律和待复核项。
- 将内置 skin manifest 升级到 `schemaVersion: 2`。
- 每个 action 支持：
  - `label`
  - `category`
  - `loopMode`
  - `priority`
  - `interruptPolicy`
  - `tags`
  - `variants.right.animation`
  - `variants.left.animation`
- `PetRuntime` 新增当前朝向、循环模式、播放序号和自动回 idle 标记。
- QML 在一次性动作播放到最后一帧时通知 `PetRuntime`，由 C++ 决定是否回到 idle。
- 右键菜单保留开发测试入口，新增切换朝向和几个更明确的动作测试项。

## 非目标

- 不实现完整 `AnimationOrchestrator`。
- 不实现动作队列、优先级抢占、阶段退出或 enter/loop/exit 子动画。
- 不实现移动路径规划。
- 不接入大模型或聊天窗口。
- 不保证所有旧 GIF 的语义都一次性标注准确。

## 设计选择

### Manifest 先升级结构，不追求完整编排

采用 manifest v2：

```json
{
  "schemaVersion": 2,
  "defaultFacing": "right",
  "facings": ["right", "left"],
  "states": {
    "idle": { "action": "idle_stand" },
    "thinking": { "action": "thinking" },
    "speaking": { "action": "objecting" }
  },
  "actions": {
    "thinking": {
      "label": "抱胸思考",
      "category": "cognitive",
      "loopMode": "loop",
      "priority": 20,
      "interruptPolicy": "replace",
      "tags": ["thinking", "cognitive"],
      "variants": {
        "right": { "animation": "qrc:/pet/thinking-right.gif" },
        "left": { "animation": "qrc:/pet/thinking-left.gif" }
      }
    }
  }
}
```

`loopMode` 先支持三种值：

- `loop`：循环动作，通常用于 idle、thinking、walking。
- `onceThenIdle`：一次性动作，播到最后一帧后回到 idle。
- `hold`：一次性或静态动作，播完后停在当前动作，后续再手动切走。

### 朝向属于 PetRuntime 的当前上下文

`PetRuntime.currentFacing` 默认从 manifest 的 `defaultFacing` 读取。播放 action 时，先查当前朝向的 variant；如果缺失，再查默认朝向；仍缺失就回退到 fallback action。

这能覆盖现在的素材规律：大部分动作左右各一份 GIF，未来移动系统只要更新 facing，再播放对应 action 即可。

### QML 只负责观察动画帧，不决定状态逻辑

Qt 官方文档说明 `AnimatedImage` 有 `currentFrame` 和 `frameCount`；`frameCount` 对部分格式可能为 0。因此 QML 只在 `frameCount > 0` 且 `currentFrame >= frameCount - 1` 时调用：

```qml
App.PetRuntime.handleAnimationFinished()
```

是否回到 idle 由 C++ 根据当前 action 的 `loopMode` 决定。这样可以避免把动作策略写进 QML。

### 重播同一动作需要 playbackSerial

如果用户连续点两次“测试异议”，`source` URL 不变，QML 未必会自动从第 0 帧重播。`PetRuntime` 每次切换或重播 action 都递增 `playbackSerial`，QML 监听它后重置 `AnimatedImage.currentFrame` 并恢复播放。

## 右键菜单开发入口

Phase 0.6 菜单保留：

- 回到待机
- 切换朝向
- 测试思考
- 测试异议
- 测试鞠躬
- 退出

这些入口只是开发验证工具，后续 Dashboard 或调试面板成熟后可以迁走。

## 测试策略

- 新增 `tests/check_phase_0_6_animation_runtime.py`：
  - 检查 manifest v2 结构。
  - 检查 action variants 含左右朝向。
  - 检查 `PetRuntime` 暴露 `currentFacing`、`currentLoopMode`、`playbackSerial`。
  - 检查 QML 监听 `currentFrame` 并调用 `handleAnimationFinished()`。
  - 检查资源盘点文档存在。
- 保留原有 Phase 0 / 0.5 静态检查。
- 继续运行：

```sh
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml
```

## 后续扩展

Phase 0.6 完成后，下一步可以继续做 Phase 0.7：

- 将随机 idle、点击、双击、拖拽晃动等交互统一接入 action 请求。
- 增加 `ActionRequest`，把来源、优先级、是否允许打断和 fallback 记录清楚。
- 为移动系统设计 `moveTo(x, y)` 和 8 方向移动动作映射。
- 把 expression/action 映射做成用户皮肤可配置字段，而不是写死在代码里。
