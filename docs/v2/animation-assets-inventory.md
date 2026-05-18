# Animation Assets Inventory

本文档是 v2 动画系统的素材盘点表。它的目的不是一次性把所有动作命名完全准确，而是先保证 **每一个 GIF 都有登记位置**，后续你可以逐项补充语义、朝向和适合的使用场景。

本文不是 manifest schema 规范。更准确的分工是：

- `skin-pack-playback-behavior-design.md`：定义正式层级模型，例如 Clip、Action、Recipe、ActionPool、BehaviorRule。
- `pet-runtime-animation-design.md`：解释 Pet Runtime 总体职责和调度原则。
- 本文：记录旧素材事实、语义标注、朝向判断和拆 phase 线索。

如果本文里写到“进入 / 循环 / 退出”或某个 GIF 的帧段范围，它的意思是“未来可以据此生成
Clip / Action / Recipe”，不是让运行时直接读取本文，也不是最终 schema 字段。

当前旧素材主要分为五类：

- `stand/`：站立/待机循环。
- `walk/`：8 方向行走循环。
- `run/`：8 方向奔跑循环。
- `once/`：数字命名的一次性动作，语义需要逐步复核。
- `special/`：旧版已按动作语义命名的特殊动作。

## 填写约定

- `当前语义` 写目前能确认或初步判断的动作含义。
- `朝向` 写 `right`、`left`、`8-direction`、`unknown` 或更具体方向。
- `播放建议` 写 `loop`、`onceThenIdle`、`hold`、`phased` 或 `unknown`。这只是迁移提示，不等同于最终 Recipe。
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

## 当前已接入运行时动作参考

这张表反映当前最小运行时已经接入的 action，并不代表最终 skin manifest 只能长这样。

| Action ID | 右向资源 | 左向资源 | 当前 loopMode | 用途 |
| --- | --- | --- | --- | --- |
| `idle_stand` | `stand/0.gif` | `stand/1.gif` | `loop` | 默认待机 |
| `thinking` | `once/4.gif` | `once/5.gif` | `loop` | 思考/等待模型回复 |
| `objecting` | `special/object0.gif` | `special/object1.gif` | `onceThenIdle` | 强调/异议/说话测试 |
| `bow` | `special/bow0.gif` | `special/bow1.gif` | `onceThenIdle` | 鞠躬/致意 |
| `tea` | `special/tea0.gif` | `special/tea1.gif` | `hold` | 坐着喝茶/休息测试 |

## 完整 GIF 清单

### stand

| 文件 | 当前语义 | 朝向 | 播放建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/stand/0.gif` | 站立待机 | right | loop | 已接入 |
| `gifs/stand/1.gif` | 站立待机 | left | loop | 已接入 |

### walk

`walk/0.gif` 到 `walk/7.gif` 是后续移动系统的核心素材。这里先逐项登记，具体方向索引等移动系统设计时再确认。

| 文件 | 当前语义 | 朝向 | 播放建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/walk/0.gif` | 行走，向右 | 8-direction | loop | 待你确认 |
| `gifs/walk/1.gif` | 行走，向左 | 8-direction | loop | 待你确认 |
| `gifs/walk/2.gif` | 行走，向右上 | 8-direction | loop | 待你确认 |
| `gifs/walk/3.gif` | 行走，向左上 | 8-direction | loop | 待你确认 |
| `gifs/walk/4.gif` | 行走，向右下 | 8-direction | loop | 待你确认 |
| `gifs/walk/5.gif` | 行走，向左下 | 8-direction | loop | 待你确认 |
| `gifs/walk/6.gif` | 行走，向上 | 8-direction | loop | 待你确认 |
| `gifs/walk/7.gif` | 行走，向下 | 8-direction | loop | 待你确认 |

### run

`run/0.gif` 到 `run/7.gif` 是后续跑动/快速移动素材。方向索引暂不写死。

| 文件 | 当前语义 | 朝向 | 播放建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/run/0.gif` | 奔跑，向右   | 8-direction | loop | 待你确认 |
| `gifs/run/1.gif` | 奔跑，向左 | 8-direction | loop | 待你确认 |
| `gifs/run/2.gif` | 奔跑，向右上 | 8-direction | loop | 待你确认 |
| `gifs/run/3.gif` | 奔跑，向左上 | 8-direction | loop | 待你确认 |
| `gifs/run/4.gif` | 奔跑，向右下 | 8-direction | loop | 待你确认 |
| `gifs/run/5.gif` | 奔跑，向左下 | 8-direction | loop | 待你确认 |
| `gifs/run/6.gif` | 奔跑，向上 | 8-direction | loop | 待你确认 |
| `gifs/run/7.gif` | 奔跑，向下 | 8-direction | loop | 待你确认 |

### once

`once/` 目录是数字命名动作。当前只确认 `once/4.gif` 和 `once/5.gif` 是抱胸思考左右朝向，其余先留给你确认。

| 文件 | 当前语义 | 朝向 | 播放建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/once/0.gif` | Confident shrugging 耸肩摊手，摇摇头，带点轻蔑嘲讽的意味 | right | onceThenIdle | 待你确认 |
| `gifs/once/1.gif` | Confident shrugging 耸肩摊手，摇摇头，带点轻蔑嘲讽的意味 | left | onceThenIdle | 待你确认 |
| `gifs/once/2.gif` | tapping head,一只手举在头边，食指摇摆轻拍脑袋 | right | onceThenIdle | 待你确认 |
| `gifs/once/3.gif` | tapping head,一只手举在头边，食指摇摆轻拍脑袋 | left | onceThenIdle | 待你确认 |
| `gifs/once/4.gif` | 抱胸思考然后放下。注：这个动作应该拆分成 进入-循环-退出 三个部分，其中1～4帧是进入，5帧～8帧是最小循环单位，44～47帧是退出（进入的倒放）。我列的这些帧数区间都是闭区间。 | right | phased | 已接入 |
| `gifs/once/5.gif` | 抱胸思考然后放下。注：这个动作应该拆分成 进入-循环-退出 三个部分，其中1～4帧是进入，5帧～8帧是最小循环单位，44～47帧是退出（进入的倒放）。我列的这些帧数区间都是闭区间。 | left | phased | 已接入 |
| `gifs/once/6.gif` | 坐着喝茶 | right | loop | 待你确认 |
| `gifs/once/7.gif` | 坐着喝茶 | left | loop | 待你确认 |
| `gifs/once/8.gif` | 看手表 | right | onceThenIdle | 待你确认 |
| `gifs/once/9.gif` | 看手表 | left | onceThenIdle | 待你确认 |
| `gifs/once/10.gif` | pointing，抬起手然后指着前方指指点点。注：这个动作应该拆分成 进入-循环-退出 三个部分，其中1～3帧是进入，4帧～7帧是最小循环单位，23～24帧是退出。我列的这些帧数区间都是闭区间。 | right | phased | 待你确认 |
| `gifs/once/11.gif` | pointing，抬起手然后指着前方指指点点。注：这个动作应该拆分成 进入-循环-退出 三个部分，其中1～3帧是进入，4帧～7帧是最小循环单位，23～24帧是退出。我列的这些帧数区间都是闭区间。 | left | phased | 待你确认 |
| `gifs/once/12.gif` | 掏手机出来接电话/打电话 | right | onceThenIdle | 待你确认 |
| `gifs/once/13.gif` | 掏手机出来接电话/打电话 | left | onceThenIdle | 待你确认 |
| `gifs/once/14.gif` | 回头看一眼身后 | right | onceThenIdle | 待你确认 |
| `gifs/once/15.gif` | 回头看一眼身后 | left | onceThenIdle | 待你确认 |
| `gifs/once/16.gif` | 低头看一眼地板/脚 | right | onceThenIdle | 待你确认 |
| `gifs/once/17.gif` | 低头看一眼地板/脚 | left | onceThenIdle | 待你确认 |
| `gifs/once/18.gif` | 抬头看一眼 | right | onceThenIdle | 待你确认 |
| `gifs/once/19.gif` | 抬头看一眼 | left | onceThenIdle | 待你确认 |

### special

`special/` 目录的文件名比 `once/` 更有语义，但有些动作可能包含 enter/loop/exit 或多朝向，需要后续逐个确认。

| 文件 | 当前语义 | 朝向 | 播放建议 | 状态 |
| --- | --- | --- | --- | --- |
| `gifs/special/back0.gif` | 一只脚后撤，有点惊讶、警惕、防御的意思。注：这个动作可以拆分成 进入-退出 两个部分，其中1～7帧是进入，8～9帧是退出。我列的这些帧数区间都是闭区间。如果要做循环区间，那就是定在第7帧单帧。但一般不需要这个循环区间，而是有可能出现停在进入动画的最后一帧，时机到了再播放推出动画，这种情况只会出现在AI聊天时，比如AI输出了一句带有慌张/防御情绪的话，这个时候可以播放进入动画，然后当用户发送下一句话、或者聊天窗被关闭时，就可以播放退出动画。 | right | phased | 待你确认 |
| `gifs/special/back1.gif`          | 一只脚后撤，有点惊讶、警惕、防御的意思。注：这个动作可以拆分成 进入-退出 两个部分，其中1～7帧是进入，8～9帧是退出。我列的这些帧数区间都是闭区间。如果要做循环区间，那就是定在第7帧单帧。但一般不需要这个循环区间，而是有可能出现停在进入动画的最后一帧，时机到了再播放推出动画，这种情况只会出现在AI聊天时，比如AI输出了一句带有慌张/防御情绪的话，这个时候可以播放进入动画，然后当用户发送下一句话、或者聊天窗被关闭时，就可以播放退出动画。 | left | phased | 待你确认 |
| `gifs/special/bow0.gif` | 鞠躬，表示感谢或尊敬 | right | onceThenIdle | 已接入 |
| `gifs/special/bow1.gif` | 鞠躬，表示感谢或尊敬 | left | onceThenIdle | 已接入 |
| `gifs/special/briefcaseIn0.gif` | 提着公文包公文包向右走。桌宠刚启动时播放，播放时桌宠会向右走。 | right | once | 待你确认 |
| `gifs/special/briefcaseStop0.gif` | 提着公文包停止/站定，然后把公文包放下，桌宠刚启动时走到落脚点时播放。 | right | onceThenIdle | 待你确认 |
| `gifs/special/crossed0.gif` | 抱胸/交叉手臂，然后说话，然后放下手臂 | right | phased | 待你确认 |
| `gifs/special/crossed1.gif` | 抱胸/交叉手臂，然后说话，然后放下手臂。双击时有概率触发。但接入AI对话功能之后，这个动作也可以被拆分为三个部分：1～4帧进入（双手抱胸），5～8帧循环（说话），循环的部分循环够了可以hold在最后一帧（闭嘴状态），然后9～11是退出（放下双臂） | left | phased | 待你确认 |
| `gifs/special/crouch0.gif` | 被吓到然后下蹲，这是晃动桌宠时特有的动画。在旧版桌宠里，晃动桌宠的逻辑有两种，如果晃动时间不长，就只会被轻度吓到，然后接standup2，如果一直晃动，就会蹲下，然后一直到松手之后，再保持蹲下一段时间，才播放standup0。这块逻辑有点小复杂，我记得不是很清楚了，你要去看旧版代码。 | right | hold | 待你确认 |
| `gifs/special/crouch1.gif` | 被吓到然后下蹲，这是晃动桌宠时特有的动画。在旧版桌宠里，晃动桌宠的逻辑有两种，如果晃动时间不长，就只会被轻度吓到，然后接standup3，如果一直晃动，就会蹲下，然后一直到松手之后，再保持蹲下一段时间，才播放standup1。这块逻辑有点小复杂，我记得不是很清楚了，你要去看旧版代码。 | left | hold | 待你确认 |
| `gifs/special/object0.gif` | 逆转裁判里提出异议时的经典动作，双击时有概率触发 | right | onceThenIdle | 已接入 |
| `gifs/special/object1.gif` | 逆转裁判里提出异议时的经典动作，双击时有概率触发 | left | onceThenIdle | 已接入 |
| `gifs/special/pickup0.gif` | 捡起检察官徽章。双击触发“看招”时专用的动画。 | right | onceThenIdle | 待你确认 |
| `gifs/special/pickup1.gif` | 捡起检察官徽章。双击触发“看招”时专用的动画。 | left | onceThenIdle | 待你确认 |
| `gifs/special/scared0.gif` | 受惊后撤 | right | onceThenIdle | 待你确认 |
| `gifs/special/scared1.gif` | 受惊后撤 | left | onceThenIdle | 待你确认 |
| `gifs/special/sleep0.gif` | 入睡动作，从站着到躺下 | right | once | 待你确认 |
| `gifs/special/sleep1.gif` | 入睡动作，从站着到躺下 | left | once | 待你确认 |
| `gifs/special/sleeping0.gif` | 睡眠中 | right | loop | 待你确认 |
| `gifs/special/sleeping1.gif` | 睡眠中 | left | loop | 待你确认 |
| `gifs/special/standup0.gif` | 从完全蹲下的状态起身 | right | onceThenIdle | 待你确认 |
| `gifs/special/standup1.gif` | 从完全蹲下的状态起身 | left | onceThenIdle | 待你确认 |
| `gifs/special/standup2.gif` | 从被轻度吓到的状态恢复 | right | onceThenIdle | 待你确认 |
| `gifs/special/standup3.gif` | 从被轻度吓到的状态恢复 | left | onceThenIdle | 待你确认 |
| `gifs/special/tea0.gif` | 坐着喝茶，然后鞠躬表示感谢。在旧版桌宠里是点击菜单项”喂食红茶“之后播放的一次性动画。其实1～13帧可以拿出来作为一个喝茶的循环phase。和tea2的区别是，tea0是喝完之后点点脚，tea2是喝完一口之后眨眨眼 | right | phased | 已接入 |
| `gifs/special/tea1.gif` | 坐着喝茶，然后鞠躬表示感谢。在旧版桌宠里是点击菜单项”喂食红茶“之后播放的一次性动画。其实1～13帧可以拿出来作为一个喝茶的循环phase。tea0和tea2的区别是，tea0是喝完之后点点脚，tea2是喝完一口之后眨眨眼 | left | phased | 已接入 |
| `gifs/special/tea2.gif` | 坐着喝茶，然后鞠躬表示感谢。在旧版桌宠里是点击菜单项”喂食红茶“之后播放的一次性动画。其实1～13帧可以拿出来作为一个喝茶的循环phase。tea0和tea2的区别是，tea0是喝完之后点点脚，tea2是喝完一口之后眨眨眼 | right | phased | 待你确认 |
| `gifs/special/tea3.gif` | 坐着喝茶，然后鞠躬表示感谢。在旧版桌宠里是点击菜单项”喂食红茶“之后播放的一次性动画。其实1～13帧可以拿出来作为一个喝茶的循环phase。tea0和tea2的区别是，tea0是喝完之后点点脚，tea2是喝完一口之后眨眨眼 | left | phased | 待你确认 |
| `gifs/special/turn0.gif` | 转身，从向右转到向左 | right | onceThenIdle | 待你确认 |
| `gifs/special/turn1.gif` | 转身，从向左转到向右 | left | onceThenIdle | 待你确认 |
| `gifs/special/wake0.gif` | 从睡眠中醒来 | right | onceThenIdle | 待你确认 |
| `gifs/special/wake1.gif` | 从睡眠中醒来 | left | onceThenIdle | 待你确认 |

## 后续整理规则

- 先在 manifest 里用稳定 action id，例如 `objecting`、`bow`、`sleeping`，不要在代码里写 `4.gif` 这种资源名。
- 同一 action 的左右朝向放在 `variants.right` / `variants.left`。
- 动作能否循环由 `loopMode` 表达，不靠文件夹名推断。
- 素材语义不确定时，在文档里标“待你确认”，不要猜成最终命名。
- 如果某个动作未来需要拆成 `enter` / `loop` / `exit`，优先在 manifest 里拆阶段，不直接改运行时接口。
