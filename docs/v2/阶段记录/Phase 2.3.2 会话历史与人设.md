# Phase 2.3.2 会话历史与人设

## 目标

本阶段落地 `docs/v2/设计方案/会话历史与人设设计.md`：皮肤人设、SQLite 会话历史、ChatService 上下文组装、摘要压缩和会话列表 UI。

## 已实现范围

- 皮肤 `persona.md` 加载与内置皮肤 override。
- 设置页角色人格编辑。
- sidecar `$MILES_DATA_DIR/chat.db` SQLite 会话存储。
- `/v1/conversations` 会话管理 API。
- `/v1/chat/messages` 使用真实 `conversationId` 并持久化 user/assistant。
- ChatService 拼装 persona、动态 EXPR 规则、摘要 system 上下文。
- 长对话触发摘要压缩，摘要中发送 `miles.chat.memory.summarizing`。
- ChatWindow 会话列表、新建、切换、删除。

## 验收

- `python3 tests/check_phase_2_3_2_session_persona.py`
- `cd apps/agent-core && go test ./...`
- `ctest --test-dir build -R "check_phase_2_3_2_session_persona|skin_manifest_loader_smoke|settings_service_smoke|chat_controller_smoke" --output-on-failure`

## 边界

- persona 始终跟随当前皮肤，sidecar 不缓存旧 persona。
- Provider 不读 store，不组装历史。
- 摘要作为 system prompt 内容，不作为 assistant 消息。
- Langfuse 可观测性留给 Phase 2.3.3。
