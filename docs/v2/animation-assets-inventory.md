# Animation Assets Inventory

本文档是 v2 动画系统的初始素材盘点。它不是最终命名表，而是给后续整理皮肤 manifest、动作标签、朝向和循环策略用的工作底稿。

当前旧素材主要分为五类：

- `stand/`：站立/待机循环。
- `walk/`：8 方向行走循环。
- `run/`：8 方向奔跑循环。
- `once/`：数字命名的一次性动作，语义需要逐步复核。
- `special/`：旧版已按动作语义命名的特殊动作。

## 朝向约定

当前已知规律：

- `stand/0.gif`：朝右站立。
- `stand/1.gif`：朝左站立。
- `once/4.gif`：朝右抱胸思考。
- `once/5.gif`：朝左抱胸思考。
- `special/*0.gif` 通常作为朝右资源。
- `special/*1.gif` 通常作为朝左资源。

这个规律还需要在逐个动作验收时继续复核。Phase 0.6 先把它写进 manifest，后续如果发现某个动作方向反了，只改 manifest/qrc alias，不改运行时接口。

## 已接入 Phase 0.6 的动作

| Action ID | 右向资源 | 左向资源 | loopMode | 用途 |
| --- | --- | --- | --- | --- |
| `idle_stand` | `stand/0.gif` | `stand/1.gif` | `loop` | 默认待机 |
| `thinking` | `once/4.gif` | `once/5.gif` | `loop` | 思考/等待模型回复 |
| `objecting` | `special/object0.gif` | `special/object1.gif` | `onceThenIdle` | 强调/异议/说话测试 |
| `bow` | `special/bow0.gif` | `special/bow1.gif` | `onceThenIdle` | 鞠躬/致意 |
| `tea` | `special/tea0.gif` | `special/tea1.gif` | `hold` | 喝茶/休息测试 |

## 行走与奔跑

`walk/0.gif` 到 `walk/7.gif`、`run/0.gif` 到 `run/7.gif` 是后续移动系统的核心素材。Phase 0.6 暂不把它们接入运行时，因为移动需要先定义坐标目标、路径规划和 8 方向映射。

初始判断：

| 分组 | 文件 | 当前语义 |
| --- | --- | --- |
| walk | `walk/0.gif` ~ `walk/7.gif` | 8 方向行走，方向索引待复核 |
| run | `run/0.gif` ~ `run/7.gif` | 8 方向奔跑，方向索引待复核 |

后续 Phase 0.7 或移动专题应补一张方向表，例如 `north`、`northEast`、`east`、`southEast`、`south`、`southWest`、`west`、`northWest` 到 GIF 的映射。

## 数字 once 动作

`once/0.gif` 到 `once/19.gif` 多数还没有可靠语义名。当前仅确认：

| 文件 | 当前语义 |
| --- | --- |
| `once/4.gif` | 朝右抱胸思考 |
| `once/5.gif` | 朝左抱胸思考 |

其余 `once` 动作先保留原始文件名，等后续通过可视化预览或人工截图确认后再加入 manifest。

## special 动作

`special/` 目录已经有较清晰的旧版命名，适合优先迁入 manifest：

| 文件 | 当前语义 |
| --- | --- |
| `special/object0.gif` / `special/object1.gif` | 异议/强调 |
| `special/bow0.gif` / `special/bow1.gif` | 鞠躬 |
| `special/crossed0.gif` / `special/crossed1.gif` | 抱胸/交叉手臂 |
| `special/crouch0.gif` / `special/crouch1.gif` | 下蹲 |
| `special/pickup0.gif` / `special/pickup1.gif` | 捡起物品 |
| `special/scared0.gif` / `special/scared1.gif` | 受惊 |
| `special/sleep0.gif` / `special/sleep1.gif` | 入睡动作 |
| `special/sleeping0.gif` / `special/sleeping1.gif` | 睡眠循环 |
| `special/standup0.gif` ~ `special/standup3.gif` | 起身，可能含多阶段/多朝向 |
| `special/tea0.gif` ~ `special/tea3.gif` | 喝茶/坐下喝茶，可能含多阶段/多朝向 |
| `special/turn0.gif` / `special/turn1.gif` | 转身 |
| `special/wake0.gif` / `special/wake1.gif` | 醒来 |
| `special/back0.gif` / `special/back1.gif` | 背向/转背，语义待复核 |
| `special/briefcaseIn0.gif` | 公文包进入动作 |
| `special/briefcaseStop0.gif` | 公文包停止/站定动作 |

## 后续整理规则

- 先在 manifest 里用稳定 action id，例如 `objecting`、`bow`、`sleeping`，不要在代码里写 `4.gif` 这种资源名。
- 同一 action 的左右朝向放在 `variants.right` / `variants.left`。
- 动作能否循环由 `loopMode` 表达，不靠文件夹名推断。
- 素材语义不确定时，在文档里标“待复核”，不要猜成最终命名。
