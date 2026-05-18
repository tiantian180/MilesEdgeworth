# Skin Pack / Playback / Behavior 设计

本文档是 v2 皮肤包、动画播放编排和交互行为的详细层级模型。它比
`pet-runtime-animation-design.md` 更具体，后续 manifest / behavior schema 的设计以本文为准。

相关文档分工：

- `pet-runtime-animation-design.md`：解释 Pet Runtime 的总体职责、调度原则和 AI/Agent 接入关系。
- `skin-pack-playback-behavior-design.md`：定义皮肤包、Clip、Action、Recipe、ActionPool、BehaviorRule 和 Custom Interaction 的层级模型。
- `animation-assets-inventory.md`：记录旧 GIF 素材事实、语义猜测、朝向和拆 phase 线索，不作为 schema 规范。

## 总体层级

```text
Assets
  -> Clips
  -> Actions
  -> Recipes
  -> ActionPools
  -> BehaviorRules / ExpressionMappings / CustomInteractions
  -> ActionRequests
  -> PetRuntime / PropRuntime / SideEffectRuntime
```

含义：

- `Assets` 是原始素材文件，例如 GIF、WebP、APNG、音频、道具图片。
- `Clip` 是最小播放片段，可以是完整文件，也可以是某个文件的帧区间。
- `Action` 是语义动作，例如 `thinking`、`objecting`、`sleep`、`walk`。
- `Recipe` 是唯一时间线编排层，描述一个动作或一个场景如何播放。
- `ActionPool` 是动作候选集合，用于随机 idle、点击、LLM 表达标签等场景。
- `BehaviorRule` 把输入事件映射到 ActionRequest。
- `CustomInteraction` 是需要代码的高级玩法，例如检察官徽章。
- `ActionRequest` 是运行时请求，PetRuntime 只接收语义请求，不直接接收 GIF 文件名。

## 文件职责

推荐皮肤包结构：

```text
skins/miles-edgeworth/
  manifest.json
  behavior.json
  interactions/
    takethat.ts
  assets/
    body/
      idle/
      locomotion/
      gestures/
      rest/
      startup/
      raw/
    props/
      prosecutor_badge/
    audio/
```

职责划分：

- `manifest.json`：素材能力，包含 canvas、anchors、clips、actions、recipes、actionPools、expressionMappings、fallback。
- `behavior.json`：默认行为，包含 idle 随机、点击区域、双击、右键菜单、agent 状态等事件到动作的映射。
- `interactions/`：可信高级交互代码，可选。普通换肤不需要写代码。
- `assets/`：正本素材。优先按资源语义组织，不按触发来源组织。

## Clip

Clip 是最小播放片段，只描述“从哪里取素材”和“取哪一段”。

```json
{
  "clips": {
    "thinking_full_right": {
      "file": "assets/body/raw/once/4.gif"
    },
    "thinking_enter_right": {
      "file": "assets/body/raw/once/4.gif",
      "frameRange": [1, 4]
    },
    "thinking_loop_right": {
      "file": "assets/body/raw/once/4.gif",
      "frameRange": [5, 8],
      "loop": true
    },
    "thinking_exit_right": {
      "file": "assets/body/raw/once/4.gif",
      "frameRange": [44, 47]
    }
  }
}
```

如果当前 Qt/QML 运行时不能直接播放 GIF 局部帧，`frameRange` 仍然作为源数据记录。
后续 asset compiler 可以把这些 clip 导出成派生 GIF/WebP/APNG。派生文件是生成物，
不是人工维护的正本。

## Action

Action 是语义动作，外部模块可以请求 Action，但不应该请求 Clip。

```json
{
  "actions": {
    "thinking": {
      "label": "抱胸思考",
      "category": "cognitive",
      "phases": {
        "enter": {
          "variants": {
            "right": { "clip": "thinking_enter_right" },
            "left": { "clip": "thinking_enter_left" }
          }
        },
        "loop": {
          "variants": {
            "right": { "clip": "thinking_loop_right" },
            "left": { "clip": "thinking_loop_left" }
          }
        },
        "exit": {
          "variants": {
            "right": { "clip": "thinking_exit_right" },
            "left": { "clip": "thinking_exit_left" }
          }
        }
      }
    }
  }
}
```

一个 Action 可以是单段动作，也可以由多个 phase 组成。常见 phase：

```text
main
enter
loop
exit
hold
```

## Recipe

Recipe 是动画系统里唯一的时间线编排层。

它既可以编排一个 Action 内部的 phase，也可以编排多个 Action、等待、条件和副作用。

```text
Action Recipe
  thinking.enter -> thinking.loop -> thinking.exit

Scenario Recipe
  drink_tea -> bow -> idle_stand
```

示例：

```json
{
  "recipes": {
    "thinking.idleOnce": {
      "scope": "action",
      "action": "thinking",
      "pattern": "enterLoopExit",
      "steps": [
        { "phase": "enter", "repeat": 1 },
        { "phase": "loop", "repeat": 10 },
        { "phase": "exit", "repeat": 1 }
      ]
    },
    "thinking.holdUntilCancelled": {
      "scope": "action",
      "action": "thinking",
      "pattern": "enterLoopExit",
      "steps": [
        { "phase": "enter", "repeat": 1 },
        { "phase": "loop", "repeat": "untilCancelled" },
        { "phase": "exit", "repeat": 1 }
      ]
    },
    "tea.drinkThenBow": {
      "scope": "scenario",
      "pattern": "sequence",
      "steps": [
        { "action": "drink_tea", "recipe": "tea.loopForDuration" },
        { "action": "bow", "recipe": "once" },
        { "action": "idle_stand", "recipe": "loop" }
      ]
    }
  }
}
```

如果次数或时长需要运行时决定，Recipe 可以保留参数位：

```json
{
  "phase": "loop",
  "durationMs": { "param": "durationMs", "default": 5000 }
}
```

运行时请求可以传：

```json
{
  "action": "drink_tea",
  "recipe": "tea.loopForDuration",
  "recipeParams": {
    "durationMs": 12000
  }
}
```

MVP 可以先只支持固定 `repeat` / `durationMs` 和 `repeat: "untilCancelled"`。

## ActionPool

ActionPool 是动作候选集合，用于多个触发来源复用同一组动作。

```json
{
  "actionPools": {
    "idle.random": [
      { "action": "check_watch", "recipe": "once", "weight": 20 },
      { "action": "shrug", "recipe": "once", "weight": 20 },
      { "action": "thinking", "recipe": "thinking.idleOnce", "weight": 10 }
    ],
    "ai.thinking": [
      { "action": "thinking", "recipe": "thinking.holdUntilCancelled", "weight": 100 }
    ]
  }
}
```

同一个 action 可以被多个 pool 复用。不要因为“随机 idle 用一次、agent thinking 用一次”
就维护两份重复素材。

## BehaviorRule

BehaviorRule 把事件映射成 ActionRequest。

```json
{
  "behaviors": {
    "pointer.singleClick": [
      {
        "when": { "hitZone": "head", "state": "idle" },
        "pool": "click.head",
        "priority": 50
      }
    ],
    "agent.thinking.started": [
      {
        "pool": "ai.thinking",
        "priority": 70,
        "interruptHint": "afterCurrent"
      }
    ],
    "menu.drinkTea": [
      {
        "recipe": "tea.drinkThenBow",
        "priority": 60
      }
    ]
  }
}
```

BehaviorRule 适合表达通用交互：

- 闲时随机动画
- 行走 / 奔跑
- 单击和双击
- 拖拽和晃动
- 右键菜单
- agent 状态动画
- LLM expression 标签动画

## Interaction Pipeline

复杂玩法用 Custom Interaction，不要硬塞进 BehaviorRule。

```text
InteractionEvent
  -> beforeDefault handlers
  -> default BehaviorRule
  -> afterDefault handlers
  -> observer handlers
```

Custom Interaction 可以写代码，但只能通过 Host API 进入系统：

```text
ctx.pet.playAction() => ActionRequest
ctx.props.spawn()    => PropCommand
ctx.sound.play()     => SideEffect
ctx.skipDefault()    => 跳过默认行为
```

检察官徽章属于 Custom Interaction：

```text
pointer.doubleClick.beforeDefault
  30% 概率触发
  播 takethat 音效
  播 takethat 动作
  spawn prosecutor_badge prop
  skip default double click

prop.prosecutor_badge.clicked
  remove badge
  request bow

prop.prosecutor_badge.expired
  request pick_badge
```

第一版可以先把 Miles 专属玩法作为内置 C++ / QML interaction 实现。未来如果开放第三方
JS/TS 脚本，必须标记为 trusted interaction，并提供权限确认。

## ActionRequest

ActionRequest 是最终送入 PetRuntime 的请求。

```json
{
  "source": "behavior",
  "action": "thinking",
  "recipe": "thinking.holdUntilCancelled",
  "recipeParams": {},
  "interruptHint": "afterCurrent",
  "priority": 70,
  "sideEffects": [
    { "type": "bubble", "text": "让我想想。" }
  ]
}
```

`interruptHint` 是请求级策略，不写进 manifest：

```text
replace       立即切换，默认值
afterCurrent  等当前 recipe 到达自然边界后切换
```

运行时只维护一个 `pendingRequest`，不是全局队列。

## 与素材盘点的关系

`animation-assets-inventory.md` 中的内容应该被理解为“迁移线索”：

```text
once/4.gif 1-4 帧是 enter，5-8 帧是 loop，44-47 帧是 exit
```

这类信息最终应该落到本文档定义的 Clip / Action / Recipe 结构里，而不是让运行时直接
读取盘点表。

如果两份文档看起来冲突：

```text
具体 schema 和层级模型：以本文为准。
某个旧 GIF 的语义和帧段备注：以 animation-assets-inventory.md 为线索。
PetRuntime 整体职责和状态调度：以 pet-runtime-animation-design.md 为准。
当前能运行的字段：以 apps/desktop/resources/skins/miles-edgeworth/manifest.json 为准。
```

## Phase 0.8 落地范围

Phase 0.8 不实现全部系统，只做第一条细线：

1. manifest 增加 `recipes` 和 `actionPools`。
2. 把当前已接入的 `idle_stand`、`thinking`、`objecting`、`bow`、`tea`、`sleep` 映射到 action recipe。
3. 添加一个最小 scenario recipe，例如 `tea.drinkThenBow`。
4. 添加 `idle.random` pool，但先只放少量动作。
5. 添加静态测试，检查 actions、recipes、pools 的引用关系不悬空。
6. 运行时只实现 `once`、`loop`、`enterLoopExit` 和最小 `sequence`。
