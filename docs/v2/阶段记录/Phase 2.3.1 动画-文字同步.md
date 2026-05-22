# Phase 2.3.1 动画-文字同步

本文记录 Phase 2.3.1 的实现范围、验收方式和已知限制。Phase 2.3.1 把 ChatController 的“立即 flush”模型升级为 5 状态门控状态机，并落地字符速率限制器和 PetRuntime 的边界通知 API。Phase 2.3 剩余两块（完整 Miles persona + SQLite 会话历史、Langfuse 接入）由 2.3.2 / 2.3.3 单独完成。

参考设计文档：`docs/v2/设计方案/AI 聊天动画编排设计.md` §5-7。

## 完成范围

- 新增 `apps/desktop/src/chat/ChatTextPacer.{h,cpp}`：QObject + QTimer 驱动的字符速率限制器，从 `SettingsService::msPerChar()` 取值，队列超过 30 字时把间隔减半。surrogate pair 一次性 emit，避免 emoji 拆字。
- ChatController 引入 `enum class ChatPhase { IDLE, BUFFERING_FOR_START, STREAMING, GATED, WAITING_FOR_ANIMATION_END }`。各状态的事件处理与设计文档 §5.3 一致。
- PetRuntime 新增 `requestBoundaryAndNotify(std::function<void()>)` 与 `requestCleanFinishAndNotify(std::function<void()>)`，二者在 2.3.1 行为一致，均在动画自然边界回调，并带 1500ms 安全超时。
- ChatController 听 `SettingsController::saved` 后同步把新 `msPerChar` 推给 pacer。
- 新增 `chat_text_pacer_smoke` ctest 覆盖基础流、CJK、backlog 加速、msPerChar 动态调整。
- 扩展 `chat_controller_smoke` 覆盖 BUFFERING_FOR_START / STREAMING / GATED / WAITING_FOR_ANIMATION_END 转移以及 cancel / error 兜底。
- 扩展 `pet_runtime_smoke` 覆盖 boundary callback 同步 / 异步路径，以及边界通知不吞掉原有动画收尾流程。
- 新增 `tests/check_phase_2_3_1_animation_sync.py` 静态契约。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
python3 tests/check_phase_2_3_1_animation_sync.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "chat_text_pacer_smoke|chat_controller_smoke|pet_runtime_smoke|settings_service_smoke|check_phase_2_3_1_animation_sync" --output-on-failure
```

手动验收：发一段需要切换 expression 的消息，观察动画到自然边界后再切表达，文字按 `msPerChar` 节奏流出；`RUN_FINISHED` 后动画再自然结束才回 idle；取消或错误时已进入 hold buffer 的文字继续吐完，不产生新幽灵消息。

## 关键决策

- **状态机直接落在 ChatController 内**：状态机只服务 ChatController 一个使用者，事件入口也集中在 `applyStreamEvent`，拆独立类暂时没有收益。
- **`requestCleanFinishAndNotify` 与 `requestBoundaryAndNotify` 在 2.3.1 行为一致**：当前还没有 phased speaking 动画，两者都是“下一次自然边界回调”。Phase 2.4 phased 动画落地后，`requestCleanFinishAndNotify` 内部会先播 `exit` phase 再回调，不改 API。
- **取消和错误时 drain hold buffer**：不丢已经到达本地、即将显示的文本。pacer 会继续按人类节奏吐完，同时 `m_cancelled` 仍阻止后续残留 SSE 新建消息。
- **`drainPendingNotifications` 不短路 PetRuntime 原有收尾流程**：边界 callback 只是通知 ChatController，`handleAnimationFinished()` 仍继续处理 `facingAfter`、recipe 下一步、pending request 和 `onceThenIdle` 回 idle。

## 当前限制

- 文字节奏只有“线性 msPerChar + backlog 半速”两段，没有按标点动态停顿。
- Pacer emit 单位是 QChar；BMP 内的 CJK / Latin 都是 1 char，surrogate pair 按对 emit。
- `requestCleanFinishAndNotify` 暂时不播 exit phase。Phase 2.4 phased 动画落地后这条路径才会有可见差异。
- 完整 Miles persona、SQLite 会话历史和 Langfuse 仍由 Phase 2.3.2 / 2.3.3 接管。

## 设计评审待处理问题

以下问题来自 Phase 2.3.1 联调和用户手测。它们先作为 `AI 聊天动画编排设计.md` 的评审输入保留；在长期方案更新前，不应继续用局部补丁扩大动画编排实现。

### 1. `RUN_FINISHED` 早于起始动画边界

现象：模型能先发出 `miles.pet.expression.requested { state: "speaking", expression: "objection" }`，但短回复场景下 `RUN_FINISHED` 会在 `BUFFERING_FOR_START` 等待当前动画干净收尾期间到达。旧实现会清空 pending expression，导致文字显示了，桌宠没有进入 `objecting` / `crossed`。

设计缺口：`AI 聊天动画编排设计.md` §5.3 只说明 `BUFFERING_FOR_START` 中 expression 和 text 如何处理，没有定义 `RUN_FINISHED during BUFFERING_FOR_START`。评审时需要明确：

- 起始 expression 是否必须保留并播放，即使回复已经结束。
- hold buffer 是否必须等起始 expression 实际请求后才放行。
- 起始 expression 播放后，是等待该 expression 动画边界再回 idle，还是允许文字结束后立即回 idle。

### 2. 边界回调可能替换当前动画

现象：`PetRuntime::handleAnimationFinished()` 在 drain pending notification 时会执行 ChatController 回调；回调可能立刻请求并播放新的 expression 动画。旧实现继续执行原动画的 finish 流程，可能把刚切出的 expression 立刻覆盖回 idle。

设计缺口：`AI 聊天动画编排设计.md` §6 定义了边界通知 API，但没有定义 callback 的重入语义。评审时需要明确：

- callback 是否允许同步请求新动画。
- 如果 callback 改变了当前 playback，旧的 `handleAnimationFinished()` 是否必须停止后续收尾逻辑。
- Runtime 是否需要统一的 playback serial / generation 机制，避免旧边界事件污染新动画。

### 3. 单个 pending expression 无法表达多段排队

当前设计在 `GATED` 中使用“覆盖待请求 expression”的模型：新的 expression 到达时覆盖上一条 pending expression，文字进入同一个 hold buffer。若模型在当前动画边界前连续输出多个 expression 段，可能出现 hold buffer 中包含多段语义文本，但最终只按最后一个 expression 播放，造成“文字和动作不对应”。

评审时需要决定是否从 `pendingExpression + holdBuffer` 升级为 segment queue：

```text
[{ expression: objection, text: "异议！..." },
 { expression: polite, text: "不过..." }]
```

每个 segment 等待自己的动画边界后再放行对应文字。若仍保留“覆盖最新 expression”的策略，需要明确这是有意降级，并写清楚会牺牲中间 expression 的同步精度。

### 4. `RUN_FINISHED` 与 `GATED` / pacer drain 的关系不清楚

设计目前主要描述 `STREAMING -> WAITING_FOR_ANIMATION_END`，但手测中可能出现：

- `RUN_FINISHED` 到达时仍在 `GATED`，还有文本停在 hold buffer。
- `RUN_FINISHED` 到达后，pacer 仍在按人类节奏吐字。
- expression 动画已经到边界，但 UI 文字尚未吐完。

评审时需要明确“回复结束”的判定到底由哪些条件共同决定：

- provider stream 已结束。
- hold buffer 已清空。
- pacer 队列已清空。
- 当前 expression 动画已到自然边界。

否则容易出现文字还在显示，桌宠已经回 idle；或桌宠还在动作，聊天 UI 已经结束 pending 状态。

### 5. `requestBoundaryAndNotify` 和 `requestCleanFinishAndNotify` 在 Phase 2.4 前后的语义

当前 2.3.1 没有 phased speaking 动画，两个接口暂时都只能等单段 GIF 的自然边界。Phase 2.4 引入 enter / loop / exit 后，两者才会有可见差异。

评审时需要补充一张从“当前单段 GIF”到“未来 phased action”的兼容表，说明：

- 单段 loop、单段 once、onceThenIdle、phased enter / loop / exit 分别如何处理 boundary 和 clean finish。
- Phase 2.3.1 的 fallback 行为是否仍然合法。
- `interruptHint` 如何影响这两个接口，例如立即切换、播完当前 once、等 loop 边界、播 exit 后切换。

### 6. 起始同步的体验取舍

目标是“expression 先到，动作尽快切，文字随动作输出”。但如果当前动画很长，严格等待 clean finish 会让用户感觉回复卡住；如果立即硬切，又违背自然收尾目标。

评审时需要明确起始同步策略：

- 是否需要针对 idle / thinking / once / loop 使用不同默认策略。
- 是否允许模型或 sidecar 通过 `interruptHint` 表达“强动作优先”。
- 安全超时应该是 ChatController 层、PetRuntime 层，还是两层都有；超时时是否需要记录 debug / warning 日志。

### 7. expression 到具体 action 的随机映射影响验收

`objection` 目前可能映射到多个动作，例如 `objecting` 或 `crossed`。这对自然感有价值，但手测时用户可能期待“异议”一定播 `objecting`。

评审时需要区分：

- expression 语义层验收：确认 `objection` 被解析并请求。
- action 选择层验收：允许按权重随机选中多个 action。
- 特定剧情 / 强语义动作是否需要 `selection: first_available` 或显式 action request，而不是走随机 expression mapping。

## 后续入口

- Phase 2.3.2：完整 Miles persona prompt + SQLite 会话历史 + 新建 / 清空会话 + 历史截断策略。
- Phase 2.3.3：Go sidecar provider 层接入 Langfuse SDK。
- Phase 2.4：phased 动画（enter / loop / exit），PetRuntime `requestCleanFinishAndNotify` 升级为播 exit 段。
