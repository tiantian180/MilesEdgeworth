# Animation Assets Inventory

本文档是 v2 动画系统的素材盘点表。它的目的不是一次性把所有动作命名完全准确，而是先保证 **每一个 GIF 都有登记位置**，后续你可以逐项补充语义、朝向和适合的使用场景。

当前旧素材主要分为五类：

- `stand/`：站立/待机循环。
- `walk/`：8 方向行走循环。
- `run/`：8 方向奔跑循环。
- `once/`：数字命名的一次性动作，语义需要逐步复核。
- `special/`：旧版已按动作语义命名的特殊动作。

## 填写约定

- `当前语义` 写目前能确认或初步判断的动作含义。
- `朝向` 写 `right`、`left`、`8-direction`、`unknown` 或更具体方向。
- `loopMode 建议` 写 `loop`、`onceThenIdle`、`hold` 或 `unknown`。
- `状态` 写 `已接入`、`待接入`、`待你确认`。
- 不确定的条目统一写 `待你确认`，不要为了好看强行猜。

## 朝向约定

当前已知规律：

- `stand/0.gif`：朝右站立。
- `stand/1.gif`：朝左站立。
- `once/4.gif`：朝右抱胸思考。
- `once/5.gif`：朝左抱胸思考。
- `special/*0.gif` 通常作为朝右资源。
- `special/*1.gif` 通常作为朝左资源。

这个规律还需要在逐个动作验收时继续复核。后续如果发现某个动作方向反了，只改 manifest/qrc alias，不改运行时接口。

## 已接入 Phase 0.6 的动作

| Action ID | 右向资源 | 左向资源 | loopMode | 用途 |
| --- | --- | --- | --- | --- |
| `idle_stand` | `stand/0.gif` | `stand/1.gif` | `loop` | 默认待机 |
| `thinking` | `once/4.gif` | `once/5.gif` | `loop` | 思考/等待模型回复 |
| `objecting` | `special/object0.gif` | `special/object1.gif` | `onceThenIdle` | 强调/异议/说话测试 |
| `bow` | `special/bow0.gif` | `special/bow1.gif` | `onceThenIdle` | 鞠躬/致意 |
| `tea` | `special/tea0.gif` | `special/tea1.gif` | `hold` | 喝茶/休息测试 |

## 完整 GIF 清单

### stand

| 文件 | 当前语义 | 朝向 | loopMode 建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/stand/0.gif` | 站立待机 | right | loop | 已接入 |
| `gifs/stand/1.gif` | 站立待机 | left | loop | 已接入 |

### walk

`walk/0.gif` 到 `walk/7.gif` 是后续移动系统的核心素材。这里先逐项登记，具体方向索引等移动系统设计时再确认。

| 文件 | 当前语义 | 朝向 | loopMode 建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/walk/0.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/1.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/2.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/3.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/4.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/5.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/6.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/walk/7.gif` | 行走，具体方向待你确认 | 8-direction | loop | 待你确认 |

### run

`run/0.gif` 到 `run/7.gif` 是后续跑动/快速移动素材。方向索引暂不写死。

| 文件 | 当前语义 | 朝向 | loopMode 建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/run/0.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/1.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/2.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/3.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/4.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/5.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/6.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |
| `gifs/run/7.gif` | 奔跑，具体方向待你确认 | 8-direction | loop | 待你确认 |

### once

`once/` 目录是数字命名动作。当前只确认 `once/4.gif` 和 `once/5.gif` 是抱胸思考左右朝向，其余先留给你确认。

| 文件 | 当前语义 | 朝向 | loopMode 建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/once/0.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/1.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/2.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/3.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/4.gif` | 抱胸思考 | right | loop | 已接入 |
| `gifs/once/5.gif` | 抱胸思考 | left | loop | 已接入 |
| `gifs/once/6.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/7.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/8.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/9.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/10.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/11.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/12.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/13.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/14.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/15.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/16.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/17.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/18.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/once/19.gif` | 待你确认 | unknown | onceThenIdle | 待你确认 |

### special

`special/` 目录的文件名比 `once/` 更有语义，但有些动作可能包含 enter/loop/exit 或多朝向，需要后续逐个确认。

| 文件 | 当前语义 | 朝向 | loopMode 建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/special/back0.gif` | 背向/转背，语义待你确认 | right? | unknown | 待你确认 |
| `gifs/special/back1.gif` | 背向/转背，语义待你确认 | left? | unknown | 待你确认 |
| `gifs/special/bow0.gif` | 鞠躬 | right | onceThenIdle | 已接入 |
| `gifs/special/bow1.gif` | 鞠躬 | left | onceThenIdle | 已接入 |
| `gifs/special/briefcaseIn0.gif` | 公文包进入动作，语义待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/briefcaseStop0.gif` | 公文包停止/站定动作，语义待你确认 | unknown | hold | 待你确认 |
| `gifs/special/crossed0.gif` | 抱胸/交叉手臂，语义待你确认 | right? | hold | 待你确认 |
| `gifs/special/crossed1.gif` | 抱胸/交叉手臂，语义待你确认 | left? | hold | 待你确认 |
| `gifs/special/crouch0.gif` | 下蹲，语义待你确认 | right? | hold | 待你确认 |
| `gifs/special/crouch1.gif` | 下蹲，语义待你确认 | left? | hold | 待你确认 |
| `gifs/special/object0.gif` | 异议/强调 | right | onceThenIdle | 已接入 |
| `gifs/special/object1.gif` | 异议/强调 | left | onceThenIdle | 已接入 |
| `gifs/special/pickup0.gif` | 捡起物品，语义待你确认 | right? | onceThenIdle | 待你确认 |
| `gifs/special/pickup1.gif` | 捡起物品，语义待你确认 | left? | onceThenIdle | 待你确认 |
| `gifs/special/scared0.gif` | 受惊，语义待你确认 | right? | onceThenIdle | 待你确认 |
| `gifs/special/scared1.gif` | 受惊，语义待你确认 | left? | onceThenIdle | 待你确认 |
| `gifs/special/sleep0.gif` | 入睡动作，语义待你确认 | right? | onceThenIdle | 待你确认 |
| `gifs/special/sleep1.gif` | 入睡动作，语义待你确认 | left? | onceThenIdle | 待你确认 |
| `gifs/special/sleeping0.gif` | 睡眠循环，语义待你确认 | right? | loop | 待你确认 |
| `gifs/special/sleeping1.gif` | 睡眠循环，语义待你确认 | left? | loop | 待你确认 |
| `gifs/special/standup0.gif` | 起身，阶段/朝向待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/standup1.gif` | 起身，阶段/朝向待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/standup2.gif` | 起身，阶段/朝向待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/standup3.gif` | 起身，阶段/朝向待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/tea0.gif` | 喝茶/坐下喝茶，语义待你确认 | right? | hold | 已接入 |
| `gifs/special/tea1.gif` | 喝茶/坐下喝茶，语义待你确认 | left? | hold | 已接入 |
| `gifs/special/tea2.gif` | 喝茶/坐下喝茶，阶段/朝向待你确认 | unknown | hold | 待你确认 |
| `gifs/special/tea3.gif` | 喝茶/坐下喝茶，阶段/朝向待你确认 | unknown | hold | 待你确认 |
| `gifs/special/turn0.gif` | 转身，语义待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/turn1.gif` | 转身，语义待你确认 | unknown | onceThenIdle | 待你确认 |
| `gifs/special/wake0.gif` | 醒来，语义待你确认 | right? | onceThenIdle | 待你确认 |
| `gifs/special/wake1.gif` | 醒来，语义待你确认 | left? | onceThenIdle | 待你确认 |

## 后续整理规则

- 先在 manifest 里用稳定 action id，例如 `objecting`、`bow`、`sleeping`，不要在代码里写 `4.gif` 这种资源名。
- 同一 action 的左右朝向放在 `variants.right` / `variants.left`。
- 动作能否循环由 `loopMode` 表达，不靠文件夹名推断。
- 素材语义不确定时，在文档里标“待你确认”，不要猜成最终命名。
- 如果某个动作未来需要拆成 `enter` / `loop` / `exit`，优先在 manifest 里拆阶段，不直接改运行时接口。
