# Phase 2.4 Phased 动画与动画链

本文记录 Phase 2.4 的范围、前置条件、交付内容和验收方式。详细的编排协议由 `docs/v2/设计方案/AI 聊天动画编排设计.md` 定义，本文只描述阶段范围。

## 1. 目标

让 thinking / speaking 动画可以维持任意长度、自然过渡，实现"聊天时桌宠边做动作边说话"的完整体验。

成功标准（用户可感知）：

1. Miles 收到消息后抬手抱胸思考（thinking enter），循环思考小动作（loop），模型开始回复时放下双臂（exit）再切到说话动画。
2. 说话动画切换时，上一段文字吐完、上一个动画播完 exit，才开始下一段。
3. 回复结束后动画自然收尾回到待机，不硬切。
4. 用户取消时已到达本地的文字继续吐完，动画干净收尾。
5. Recipe 多步动画链能按顺序执行，支持 per-phase step 控制（如 thinking 的 enter/loop/exit 分段）。

## 2. 前置条件

- Phase 2.3 全部完成（ChatController 状态机、ChatTextPacer、PetRuntime 通知 API、会话历史、Langfuse 已落地）。
- 设计文档 `AI 聊天动画编排设计.md` 已更新至包含 segment queue、双条件门控、lifecycle 事件、cleanFinish postcondition。

## 3. 交付范围

### 3.1 前置修复：settings reload 不重置行为

当前 `reloadActiveSkin()` 总是播一遍启动序列。需要拆为两个模式：

| 模式 | 触发场景 | 行为 |
| --- | --- | --- |
| Full reload | 应用启动、切换皮肤 | 播启动序列 |
| Preserve reload | 设置保存后 sidecar 重启 | 静默刷新 manifest + 重启 sidecar，保持当前动画 |

### 3.2 PetRuntime 升级

**onceThenHold loopMode**：

- schema 新增 `onceThenHold` 值（皮肤包播放行为设计 + manifest JSON Schema）。
- `PetSurfaceWindow` 新增 onceThenHold 处理：播完最后一帧后暂停（类似 `hold`，但只播一次 enter）。
- Miles manifest 迁移：`objecting`、`bow` 从 `onceThenIdle` 改为 `onceThenHold`。

**cleanFinish 统一**：

- 合并 `requestBoundaryAndNotify` 和 `requestCleanFinishAndNotify` 为单一 `requestCleanFinishAndNotify`。
- 实现机制：等安全点 → 播 exit（如有）→ 回调。见设计文档 §8。
- Postcondition：回调时 PetRuntime 停在 "action 结束" 状态，调用方必须 `requestExpression()` 或 `returnToIdle()`。3000ms 无操作自动 returnToIdle（仅在无活跃 reply session 时启用，见设计文档 §8.1）。
- callback slot 用 `Q_ASSERT` 防止重复注册。

**Phased action 播放**：

- enter / loop / exit 三段通过 `nextPhase` 自动转移（现有骨架已有）。
- 运行时帧段播放：QMovie `jumpToFrame()` + `frameChanged()` 播放 GIF 子段。需要验证 QMovie 对 `jumpToFrame` 的支持情况，如有问题可 fallback 到预切分文件。
- enter → loop 自动转移不触发 cleanFinish 回调。

### 3.3 SSE 事件拆分

- Go sidecar 将 `miles.pet.expression.requested` 拆为两个事件：
  - `miles.pet.expression.requested`：文字段事件，只在解析到 `[EXPR:tag]` 时发出，后面跟 `TEXT_MESSAGE_CONTENT`。
  - `miles.pet.lifecycle`：非文字状态变化（`state: "thinking"`）。
- 移除流结束时的 `idle` expression 事件（`provider.go:365`），由 `RUN_FINISHED` 处理。
- Qt `ChatController` 分别处理两类事件：expression.requested 入 segment queue，lifecycle 直接请求 PetRuntime。

### 3.4 ChatController 状态机升级

**Segment queue**：

- 替换单个 `m_pendingExpression` + `m_holdBuffer` 为 `QList<ExpressionSegment>` 分段队列。
- 每个 segment 带 `segmentId`。

**双条件门控**（GATED 状态）：

- 切换到下一段需要两个条件同时满足：
  1. `cleanFinishReady`：当前动画已干净收尾。
  2. `segmentDrained(segmentId)`：速率限制器已吐完当前段所有文字。
- 用 `m_animationReady` + `m_textDrained` 两个标志追踪。

**Reply session**：

- 结束条件为 4 个条件全部满足：stream 结束、segment queue 空、pacer 空、动画 cleanFinish 完成。

**安全超时**：

- BUFFERING_FOR_START 新增 3000ms start timeout。
- GATED gate timeout 从 800ms 调整为 2000ms（覆盖安全点等待 + exit 播放）。

### 3.5 速率限制器升级

- `ChatTextPacer` 新增 `segmentId` 追踪：`append(text, streamId, segmentId)` 接口，保留现有 `streamId` + `discardBeforeStream` 防旧回复泄漏机制。
- 当前 segmentId 的文字全部吐完后 emit `segmentDrained(segmentId)` 信号。
- 整个队列为空时 emit `pacerEmpty()` 信号（`WAITING_FOR_ANIMATION_END` 依赖此信号）。
- 现有 `msPerChar`、积压追平机制不变。

### 3.6 技术债务清理

`技术债务与评审待办.md` 中的 9 个 [P2.4] 项目全部在本阶段解决，通过以上交付覆盖：

| 技术债 | 解决方式 |
| --- | --- |
| RUN_FINISHED 早于起始动画边界 | BUFFERING_FOR_START 增加 start timeout |
| 边界回调可能替换当前动画 | cleanFinish callback 单 slot + Q_ASSERT 防重复 |
| 单个 pending expression 无法多段排队 | segment queue 替换 |
| RUN_FINISHED 与 GATED / pacer drain 关系 | 双条件门控 + reply session 结束条件 |
| boundary 和 cleanFinish 语义 | 合并为统一 cleanFinish |
| 起始同步的体验取舍 | thinking lifecycle 事件 + BUFFERING_FOR_START 处理 |
| expression 到 action 的随机映射 | ExpressionMappingResolver 已支持确定性测试（固定 randomValue） |
| 取消 / 错误后的 drain 语义 | segment queue 全部 drain 到 pacer，不丢文字 |
| 动画门控超时固定值 | 调整为 2000ms，加 start timeout 3000ms |

### 3.7 Recipe steps 动画链执行

当前运行时已支持简单的多步 recipe 顺序执行（`startup.briefcase`：briefcase_in → briefcase_stop → idle_stand）。本阶段扩展 recipe step 能力，使其支持 phased action 的分段控制。

**Per-phase recipe steps**：

recipe step 新增 `phase` 字段，支持播放 action 的指定阶段：

```json
{
  "thinking.holdUntilCancelled": {
    "scope": "agent",
    "steps": [
      { "action": "thinking", "phase": "enter" },
      { "action": "thinking", "phase": "loop", "duration": "runtime" },
      { "action": "thinking", "phase": "exit" }
    ]
  }
}
```

- `"phase": "enter"` — 只播 action 的 enter 段，播完后自动进入下一步。
- `"phase": "loop"` — 播 action 的 loop 段。如果有 `"duration": "runtime"`，则由运行时控制何时结束（通过 cleanFinish 触发推进到下一步）。
- `"phase": "exit"` — 只播 action 的 exit 段，播完后自动进入下一步或结束 recipe。

**cleanFinish 与 recipe steps 的交互**：

- `requestCleanFinishAndNotify` 作用于当前 recipe step，而非整个 recipe。如果当前 step 是 `"duration": "runtime"` 的 loop step，cleanFinish 结束该 step 并推进到下一步（exit step）。exit step 播完后触发 cleanFinish 回调。
- 如果当前 step 不是 runtime-controlled，cleanFinish 等当前 step 播完后再推进。

**验收关注点**：现有 `startup.briefcase` 多步 recipe 不应被破坏。新增 per-phase step 需要对应的 manifest 声明和帧段定义。

## 4. 非目标

- 动画资源预切分工具（Pet Skin Studio 阶段做）。
- 移动 API（Phase 2.5）。
- 多模态（Phase 2.6）。

## 5. 验收

自动检查：

```bash
# Qt 编译 + 测试
cmake --build build && ctest --test-dir build --output-on-failure

# Go sidecar 测试
cd apps/agent-core && go test ./...

# Phase 验收脚本（待实现）
python3 tests/check_phase_2_4_phased_animation.py
```

手动检查：

1. **thinking 动画**：发送消息后 Miles 抬手抱胸思考（enter），循环思考（loop），模型开始回复时放下双臂（exit），然后切到说话动画。
2. **entry-only 动画**：objecting 播完后定在最后一帧（不自动回 idle），直到下一个 expression 到来直接切换。
3. **多段 expression 同步**：objecting 的文字全部吐完后，才切到 bow 动画并开始吐 polite 的文字。不会出现动画切了但文字还在吐上一段的情况。
4. **回复结束收尾**：RUN_FINISHED 后当前动画播 exit（如有），然后回到 idle。
5. **用户取消**：取消后已到达的文字继续吐完，动画播 exit 后回 idle。
6. **设置保存**：修改设置后 sidecar 重启，Miles 不播启动序列。
7. **lifecycle 事件**：Go sidecar 在流开始时发 `miles.pet.lifecycle { state: "thinking" }`，不再发结束时的 idle expression 事件。
8. **动画链（Recipe steps）**：startup.briefcase 多步 recipe 仍正常执行（briefcase_in → briefcase_stop → idle_stand 顺序播放）。per-phase recipe step（如 thinking 的 enter/loop/exit 分段控制）正常工作。

## 6. 已知限制

- 运行时帧段播放依赖 QMovie `jumpToFrame()` 的实现质量。如果特定平台有问题，可能需要 fallback 到预切分 GIF 文件。
- onceThenHold 是新增 loopMode，旧版 manifest 的 `onceThenIdle` action 不受影响。
- Per-phase recipe step 需要 action 有对应的帧段定义（`frameRange`），未声明帧段的 action 不支持 `"phase": "enter"` 等指定。
- `"duration": "runtime"` 只在 loop phase step 上有意义，其他 phase step 会自动忽略此字段。
