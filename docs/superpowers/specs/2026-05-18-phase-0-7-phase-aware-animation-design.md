# Phase 0.7 Phase-Aware Animation Runtime Design

## 背景

Phase 0.6 已经支持 action 元数据、左右朝向 variants、`loopMode` 和一次性动作回 idle。你整理素材后出现了更重要的问题：很多 GIF 不是完整语义动作，而是某个动作的前半段、保持段或后半段。例如 `sleep0.gif -> sleeping0.gif -> wake0.gif`，以及 `briefcaseIn0.gif -> briefcaseStop0.gif`。

因此 Phase 0.7 的目标不是继续堆 action，而是把运行时模型升级为：

```text
Action
  └─ Phase
      └─ Variant
          └─ Clip / GIF
```

## 目标

- manifest 支持 `phases`，每个 phase 有自己的 `loopMode`、`variants` 和可选 `nextPhase`。
- `PetRuntime` 暴露 `currentPhaseId`。
- `PetRuntime` 支持 phase 流程：
  - `enter` 播完自动进入 `loop`。
  - `loop` 保持，直到外部请求切走。
  - `exit` 播完回 idle。
- 先用睡觉动作做最小验证：
  - `sleep.enter`：`special/sleep0.gif` / `special/sleep1.gif`
  - `sleep.loop`：`special/sleeping0.gif` / `special/sleeping1.gif`
  - `sleep.exit`：`special/wake0.gif` / `special/wake1.gif`
- 右键菜单增加“测试睡觉”，点击“回到待机”时如果当前 action 有 exit phase，先播放 exit，再回 idle。

## 非目标

- 不实现 GIF 局部帧循环。
- 不拆 `once/4.gif` 的帧段。
- 不实现动作队列、优先级调度或复杂 interrupt policy。
- 不实现移动路径规划和 briefcase 启动走路。

## Manifest 结构

Phase-aware action 示例：

```json
{
  "sleep": {
    "label": "睡觉",
    "category": "idle",
    "priority": 10,
    "tags": ["sleep", "idle"],
    "initialPhase": "enter",
    "exitPhase": "exit",
    "phases": {
      "enter": {
        "loopMode": "once",
        "nextPhase": "loop",
        "variants": {
          "right": { "animation": "qrc:/pet/sleep-right.gif" },
          "left": { "animation": "qrc:/pet/sleep-left.gif" }
        }
      },
      "loop": {
        "loopMode": "loop",
        "variants": {
          "right": { "animation": "qrc:/pet/sleeping-right.gif" },
          "left": { "animation": "qrc:/pet/sleeping-left.gif" }
        }
      },
      "exit": {
        "loopMode": "onceThenIdle",
        "variants": {
          "right": { "animation": "qrc:/pet/wake-right.gif" },
          "left": { "animation": "qrc:/pet/wake-left.gif" }
        }
      }
    }
  }
}
```

旧的单段 action 继续兼容。它们可以暂时保留 `variants` + `loopMode`，运行时会把它们视为隐式 `single` phase。

## 运行时规则

- `playAction("sleep")`：进入 action 的 `initialPhase`，即 `enter`。
- `handleAnimationFinished()`：
  - 如果当前 phase 有 `nextPhase`，切到该 phase。
  - 否则如果当前 phase/action 标记 `onceThenIdle`，回到 idle。
  - 否则不处理。
- `returnToIdle()`：
  - 如果当前 action 有 `exitPhase`，且当前 phase 不是 exit，先播放 exit。
  - 否则直接切 idle。
- `toggleFacing()`：
  - 保持当前 action 和 phase，只换当前 phase 的对应朝向 variant。

## 验证

- 静态测试检查 manifest 里存在 `sleep.phases.enter/loop/exit`。
- 静态测试检查 `PetRuntime` 有 `currentPhaseId`、`playPhase`、`testSleep`。
- QML lint 必须无警告。
- 手动验证：
  - 右键“测试睡觉”：先入睡，然后进入睡眠循环。
  - 右键“回到待机”：先醒来，然后回到站立。
  - 睡觉时切换朝向：当前 phase 换成对应方向资源。
