# Pet Runtime 与动画调度设计

本文档描述 MilesEdgeworth v2 的桌宠运行时、动画调度、移动控制、换肤扩展和旧版行为还原方案。

这部分是 v2 的核心复杂点。目标不是简单把旧版 `setFileName(":/gifs/once/4.gif")` 搬到新项目里，而是把旧版手感抽象成可维护、可换肤、可被 AI/Agent 驱动的运行时系统。

## 1. 设计目标

Pet Runtime 要满足以下目标：

- 尽量还原旧版随机动作、走路/跑步、点击、双击、拖拽晃动、喝茶、睡觉、检察官徽章和音效的手感。
- 支持 AI 聊天状态：thinking、speaking、idle、error。
- 支持由当前皮肤动态声明 expression tags，而不是项目写死一组情绪词。
- 支持移动驱动动画：先决定移动目标和路线，再根据方向播放移动动画。
- 支持局部循环和阶段动画：enter / loop / exit。
- 支持左右朝向、8 方向移动和 fallback。
- 支持换肤，不要求每个皮肤都实现 Miles 的全部动作。
- 保持调度模型可理解，MVP 不做全局动画队列。

## 2. 旧版行为归纳

旧版动画大致有几类来源：

```text
移动动画：walk / run，8 个方向
空闲随机动画：stand 状态下随机切 once、walk、run、turn
鼠标交互：单击、双击、拖拽晃动
右键菜单：喝茶、睡觉、禁止自动移动、置顶、语言等
特殊副作用：检察官徽章、音效
```

旧版方向约定：

```text
direction = 0：右
direction = 1：左
walk/run 方向 = 0..7，分别表示水平、斜向和上下移动
```

很多 once/special 动作使用成对 GIF：

```text
once/4.gif：朝右的抱胸思考动作
once/5.gif：朝左的抱胸思考动作
```

旧版代码里常见写法：

```text
direction + 4
direction + 8
direction + 10
```

v2 不继续依赖数字和奇偶隐含语义，而是在 manifest 中显式声明动作名和朝向变体。

## 3. 核心概念

### PetState

程序层面的稳定状态。它们不随皮肤变化：

```text
idle
moving
thinking
speaking
error
waiting_permission
tool_running
dragging
sleeping
```

这些状态用于 Qt、Go 和 Pet Runtime 协作。它们不是角色情绪。

### ExpressionTag

皮肤声明的表达标签，用来描述语气、姿态或角色特有表达。

例子：

```text
objection
confident
polite
tea_break
checking_time
bowing
annoyed
neutral
```

ExpressionTag 不是项目写死的全局枚举。不同皮肤可以声明不同标签。模型只能从当前皮肤暴露的标签中选择，未知标签降级为 `neutral`。

ExpressionTag 只表示“想表达什么”，不等于具体动作。比如 `annoyed` 可以表现为抱胸、指指点点、看表或转身，具体选哪个由当前皮肤的映射规则决定。

### ExpressionMapping

ExpressionMapping 描述 ExpressionTag 到 Action 的映射关系。

它由皮肤 manifest 定义，Pet Runtime 执行。模型只选择 expression，不直接选择 action。这样可以做到：

```text
同一个 expression 可以对应多个 action。
不同皮肤可以用不同 action 表达同一个 expression。
同一皮肤可以按权重、轮询或上下文选择 action。
换肤时不需要修改模型 prompt 以外的项目代码。
```

例如 `annoyed` 可以映射为：

```text
annoyed
=> 50% crossed_thinking
=> 30% objecting
=> 20% check_watch
```

Pet Runtime 收到 `state=speaking + expression=annoyed` 后，会先过滤当前状态不可用的 action，再按映射策略选择一个。

建议支持的选择策略：

```text
first_available    选择第一个当前可用 action
weighted_random    按权重随机
round_robin        轮流选择，减少重复感
contextual         后续扩展，根据状态、位置、最近动作等上下文选择
```

MVP 优先实现 `first_available` 和 `weighted_random`。`round_robin` 和 `contextual` 可以后置。

### ActionRequest

外部模块发给 Pet Runtime 的请求，表示“想让桌宠做什么”。

外部模块不直接指定 GIF 文件。

建议字段：

```json
{
  "source": "chat",
  "state": "speaking",
  "expression": "objection",
  "desiredAction": "emphasis",
  "priority": 60,
  "expiresInMs": 2000,
  "sideEffects": [
    { "type": "bubble", "text": "异议！这个推论还有漏洞。" }
  ]
}
```

### Action

Action 是有语义的动作单元，例如：

```text
idle_stand
crossed_thinking
objecting
drink_tea
sleep
run
walk
objection_badge_combo
```

外部可以请求 Action，但不应该请求内部 Clip。

### Clip

Clip 是最小可复用播放片段，只负责播放素材。

Clip 可以来自：

```text
完整 GIF
完整 WebP/APNG
一个 GIF 的局部帧段
序列帧
未来其他动画格式
```

拆分规则：

```text
如果一段动画会被多个动作复用，拆成 Clip。
如果一段动画有独立语义，定义成 Action。
如果只是某个 Action 的内部步骤，只作为 Clip/Phase，不暴露给外部请求。
```

### Phase

一个 Action 内部的阶段。

常见阶段：

```text
enter
loop
exit
main
```

例如抱胸思考：

```text
enter：抬手抱胸
loop：抱胸思考
exit：放下双臂
```

### Facing

桌宠当前朝向或移动方向。

建议基础值：

```text
left
right
north
south
north_east
north_west
south_east
south_west
default
```

对于只有左右朝向的动作，使用 `left/right`。对于移动动画，可使用 8 方向。

### SideEffect

动作附带的副作用，例如：

```text
sound
bubble
badge
shake
```

例如双击 “异议” 不是单个身体动画，它可能包含：

```text
body action: objecting
sound: objection
accessory: prosecutor_badge_throw
```

### BehaviorProfile

BehaviorProfile 描述某个皮肤或角色的行为习惯。

它回答：

```text
空闲时怎么随机动？
点击头部触发什么？
点击胸部触发什么？
双击触发什么？
右键菜单“喝茶”对应什么 Action？
旧版随机概率如何迁移？
```

Miles 默认 behavior profile 用来还原旧版手感。

## 4. Manifest 建议结构

皮肤能力建议拆成两个文件：

```text
manifest.json：素材、Action、Clip、ExpressionTag、ExpressionMapping、Facing、fallback
behavior.json：随机行为、点击映射、菜单动作映射
```

示例：

```json
{
  "id": "miles-edgeworth",
  "name": "Miles Edgeworth",
  "expressions": [
    {
      "id": "objection",
      "label": "异议",
      "description": "强烈反驳、指出漏洞、语气锐利时使用",
      "allowedStates": ["speaking"],
      "priority": 90
    },
    {
      "id": "neutral",
      "label": "默认",
      "description": "没有更合适表达时使用"
    }
  ],
  "clips": {
    "crossed_thinking_right_full": {
      "file": "animations/once/4.gif",
      "frameRange": [0, 36]
    },
    "crossed_thinking_left_full": {
      "file": "animations/once/5.gif",
      "frameRange": [0, 36]
    }
  },
  "actions": {
    "crossed_thinking": {
      "kind": "oneshot",
      "variants": {
        "right": "crossed_thinking_right_full",
        "left": "crossed_thinking_left_full"
      },
      "after": "return_previous"
    }
  }
}
```

ExpressionTag 到 Action 的映射也应写在 manifest 中：

```json
{
  "expressionMappings": {
    "annoyed": {
      "selection": "weighted_random",
      "fallback": "neutral",
      "actions": [
        {
          "action": "crossed_thinking",
          "weight": 50,
          "allowedStates": ["idle", "speaking"]
        },
        {
          "action": "objecting",
          "weight": 30,
          "allowedStates": ["speaking"]
        },
        {
          "action": "check_watch",
          "weight": 20,
          "allowedStates": ["idle"]
        }
      ]
    }
  }
}
```

上例中，如果当前是 `speaking`，候选 action 是 `crossed_thinking` 和 `objecting`；如果当前是 `idle`，候选 action 是 `crossed_thinking` 和 `check_watch`。如果过滤后没有可用 action，则走 `fallback`。

后续如果同一 GIF 标出局部循环段，可以改成：

```json
{
  "actions": {
    "crossed_thinking": {
      "kind": "phased",
      "phases": {
        "enter": {
          "right": { "clip": "crossed_thinking_right_full", "frameRange": [0, 8] },
          "left": { "clip": "crossed_thinking_left_full", "frameRange": [0, 8] }
        },
        "loop": {
          "right": { "clip": "crossed_thinking_right_full", "frameRange": [9, 24], "loop": true },
          "left": { "clip": "crossed_thinking_left_full", "frameRange": [9, 24], "loop": true }
        },
        "exit": {
          "right": { "clip": "crossed_thinking_right_full", "frameRange": [25, 36] },
          "left": { "clip": "crossed_thinking_left_full", "frameRange": [25, 36] }
        }
      }
    }
  }
}
```

## 5. PlaybackKind

不同素材适合不同播放方式。不要强迫所有动画都有 enter/loop/exit。

建议类型：

```text
oneshot          单次动作，播完结束
loop             完整循环动作
phased           enter / loop / exit 阶段动画
hold_last_frame  播完后停在最后一帧
hold_frame       播到指定帧后停住
sequence         由多个 Action 或 Clip 顺序组成的高级动作
```

选择规则：

```text
如果动作有自然循环段，使用 phased。
如果动作本身就是循环，使用 loop。
如果动作是强调/互动，使用 oneshot。
如果动作能停在某帧表达状态，使用 hold_frame 或 hold_last_frame。
如果素材不适合长时间状态，只作为插入动作，不作为 thinking/speaking 主循环。
```

## 6. 调度模型

MVP 不做全局动画队列。

运行时只维护：

```text
currentAction
pendingRequest，最多一个
currentAction 内部 phase/sequence
```

原因：

- 全局队列很容易堆积无意义的空闲动作。
- 新的高优先级事件到来时，很难解释队列该保留还是清空。
- 桌宠更需要即时反应，而不是严格执行历史动作列表。

### TransitionDecision

当新的 ActionRequest 到来时，调度器做一次仲裁：

```text
decideTransition(currentAction, newRequest) -> TransitionDecision
```

TransitionDecision 描述这次冲突如何处理，而不是描述单个动画的属性。

建议决策：

```text
switch_now          立即切换，清空 pendingRequest
wait_current_done   当前动作播完后切换到 newRequest
exit_then_switch    当前 phased 动作先走 exit，再切换到 newRequest
drop_new_request    丢弃新请求
reject              拒绝请求，例如权限不允许或状态不合法
```

示例：

```text
speaking_loop + idle_random_tea => drop_new_request
thinking_loop + speaking => exit_then_switch
objecting_oneshot + user_dragging => switch_now
drink_tea_oneshot + sleep => wait_current_done
```

`pendingRequest` 只有一个槽位。新请求如果更重要，可以覆盖旧的 pendingRequest。

```text
switch_now:
  停止 currentAction
  清空 pendingRequest
  播放新请求

wait_current_done:
  pendingRequest = newRequest
  currentAction 播完后播放 pendingRequest

exit_then_switch:
  currentAction 进入 exit phase
  pendingRequest = newRequest

drop_new_request:
  什么也不做
```

后续如果真的需要连续动作，例如“转身 -> 跑到目标 -> 异议 -> 丢徽章”，应设计成一个 Action 内部的 `sequence`，而不是全局队列。

## 7. 优先级建议

不同来源的默认优先级：

```text
用户拖拽 / 手动操作
> 权限确认 / 严重错误
> 菜单强制动作
> 明确移动指令
> 聊天 speaking
> 聊天 thinking
> 鼠标点击互动
> 空闲随机动作
```

优先级不是唯一规则，还需要结合：

- 当前动作是否可打断。
- 当前动作是否有 exit 阶段。
- 新请求是否过期。
- 新请求是否允许等待。
- 当前状态是否允许该 action。

Action 可以声明自身可打断能力：

```json
{
  "interruptibility": {
    "canInterrupt": true,
    "minPlayMs": 300,
    "preferExitPhase": true
  }
}
```

## 8. 移动系统

旧版是动画驱动移动：

```text
播放 walk/run GIF
=> 每一帧 frameChanged 时窗口移动一小段
```

v2 改成移动驱动动画：

```text
收到移动目标
=> 计算路径
=> 每个 tick 更新位置
=> 根据运动方向选择 walk/run 动画
```

建议模块：

```text
MotionController
  moveTo(target, mode)
  wander()
  stop()
  update(deltaTime)
```

状态：

```text
position
target
path
velocity
facing
movementState: idle | walking | running | dragging | interrupted
```

MVP 算法：

```text
当前位置 -> 目标点，先走直线
每 16ms 或 33ms 计算方向向量
把方向量化成 8 个方向
按 walk/run 速度移动窗口
根据方向选择 walk/run 动画变体
接近目标点后吸附到目标点并切 idle
```

8 方向映射：

```text
east        -> walk/run east
west        -> walk/run west
north_east  -> walk/run north_east
north_west  -> walk/run north_west
south_east  -> walk/run south_east
south_west  -> walk/run south_west
north       -> walk/run north
south       -> walk/run south
```

后续大模型不能直接操控像素级移动，而是通过受控命令：

```json
{
  "tool": "pet.moveTo",
  "args": {
    "target": "screen_bottom_right",
    "mode": "run"
  }
}
```

Go Core 校验后发给 Qt Pet Runtime。Qt 决定路径、边界和动画。

## 9. AI / Agent 接入

模型不直接指定 GIF，不直接指定 Clip，也不应该知道旧版数字文件名。

模型只输出语义：

```json
{
  "state": "speaking",
  "expression": "objection",
  "bubble": "异议！这个推论还有漏洞。"
}
```

ExpressionTag 来自当前皮肤 manifest。Go Core 在会话开始或皮肤切换后，把当前可用 expression tags 注入提示词：

```text
当前皮肤支持这些表达标签：
- objection：强烈反驳、指出漏洞时使用
- polite：礼貌回应、致意、道歉时使用
- neutral：没有更合适表达时使用
```

Go Core 校验模型输出：

```text
如果 tag 存在，转成 miles.pet.expression.requested
如果 tag 不存在，降级为 neutral
如果模型没有输出 tag，使用 state 默认表达
```

Qt Pet Runtime 根据 state、expression、当前 facing、当前 action 和 manifest 选择最终动画。

## 10. 旧版手感还原

v2 需要一份 v1 parity checklist。至少包括：

```text
启动默认动画是否接近旧版
站立时是否会随机切 once / walk / run / turn
随机概率是否接近旧版
walk/run 速度是否接近旧版
8 方向动画是否正确
左右朝向是否正确
单击头部、小臂、胸部、肚子、脚部是否触发对应动作
双击是否触发异议和徽章
拖拽晃动是否触发害怕动作
右键菜单喝茶是否正常
右键菜单睡觉/唤醒是否正常
音效语言和静音策略是否正常
透明异形点击区域是否正常
```

旧版素材重命名或标注建议：

```text
once/4.gif -> crossed_thinking_right
once/5.gif -> crossed_thinking_left
special/object0.gif -> objecting_right
special/object1.gif -> objecting_left
walk/0.gif -> walk_east
walk/1.gif -> walk_west
run/0.gif -> run_east
run/1.gif -> run_west
```

实际文件可以暂时不重命名，先在 manifest 中建立语义映射，减少初期 diff。

## 11. Fallback 规则

皮肤不需要实现所有能力。Pet Runtime 应按优先级 fallback。

Expression 到 Action 的解析顺序建议为：

```text
1. 找当前 expression 的 expressionMappings。
2. 按当前 PetState 过滤 allowedStates 不匹配的 action。
3. 过滤当前皮肤不存在、当前 facing 不可用、当前播放类型不适合的 action。
4. 按 selection 策略选择 action。
5. 如果没有可用 action，使用 expressionMappings.fallback。
6. 如果 fallback 仍不可用，使用 neutral 或 state 默认 action。
```

示例：

```text
expression=objection + state=speaking
=> 找 speaking + objection
=> 找 objection emphasis
=> 找 confident emphasis
=> 找 speaking default
=> 找 idle default
```

朝向 fallback：

```text
exact facing
=> left/right
=> default
```

播放类型 fallback：

```text
phased thinking
=> loop thinking
=> hold_frame thinking
=> idle loop
```

未知 expression：

```text
降级为 neutral，不报错
```

## 12. MVP 范围

Phase 0 / 1 只实现必要能力：

- `loop`
- `oneshot`
- 左右朝向变体。
- 8 方向移动动画。
- `currentAction + pendingRequest`。
- 空闲随机动作。
- 鼠标交互。
- 检察官徽章和音效。

高级能力后置：

- `phased` enter / loop / exit。
- 单 GIF 局部帧段循环。
- 可视化帧段校准工具。
- 复杂跨屏寻路。
- 完整 action sequence 编辑。
- 完整换肤管理 UI。

## 13. 文档维护规则

动画系统文档必须随实现维护。

- 新增 ActionRequest 字段时，更新本文档。
- manifest 字段变化时，更新 schema 和示例。
- 发现旧版特殊逻辑时，补充 v1 parity checklist。
- 修改调度规则时，说明对旧版手感和 AI 行为的影响。
- 实现和文档冲突时，要么修实现，要么更新文档，不能长期放任。

代码注释中文为主。尤其是调度、切换、fallback、跨平台窗口和素材兼容逻辑，必须写清楚“为什么这样做”。
