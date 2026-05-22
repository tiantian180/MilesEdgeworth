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

## 后续入口

- Phase 2.3.2：完整 Miles persona prompt + SQLite 会话历史 + 新建 / 清空会话 + 历史截断策略。
- Phase 2.3.3：Go sidecar provider 层接入 Langfuse SDK。
- Phase 2.4：phased 动画（enter / loop / exit），PetRuntime `requestCleanFinishAndNotify` 升级为播 exit 段。
