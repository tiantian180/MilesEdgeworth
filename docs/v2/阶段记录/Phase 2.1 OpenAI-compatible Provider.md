# Phase 2.1 OpenAI-compatible Provider

本文记录 Phase 2.1 的实现范围、验收方式和后续限制。Phase 2.1 接入真实 OpenAI-compatible Chat Completions provider，引入 `[EXPR:tag]` 标记协议骨架，并为 Phase 2.3 的完整状态机准备 hold buffer 雏形。

参考设计：`docs/v2/设计方案/AI 聊天动画编排设计.md` §11 Phase 2.1 行。

## 完成范围

- 新增 `internal/chat/config`：从 `MILES_PROVIDER_BASE_URL` / `MILES_PROVIDER_API_KEY` / `MILES_PROVIDER_MODEL` / `MILES_PROVIDER_TEMPERATURE` / `MILES_PROVIDER_MAX_TOKENS` 环境变量读取 provider 配置；`Enabled()` 判断是否齐备。
- 新增 `internal/chat/expression`：`[EXPR:tag]` 流式 token 解析器，支持跨 chunk 边界、未知 tag 降级到 `neutral`、`Flush` 时残留作为 text 输出。
- 新增 `internal/chat/openai`：OpenAI-compatible provider 适配器，POST `/v1/chat/completions` with stream=true，构建带 expression 清单的 system prompt，把 upstream SSE delta 通过 expression parser 转换为我们的 AG-UI envelope。
- `main.go` 根据 `config.Enabled()` 选择 provider：齐备时是 openai-compatible，否则 fallback 到 mock 并在 `/health` 标 `provider: "mock-fallback"`。
- `Request` 结构增加显式 JSON tag 和 `Expressions []ExpressionInfo` 字段。
- `api.Server` 接受 provider label，加 `http.MaxBytesReader`（1MB 限制），共享常量 `DefaultListenAddr = "127.0.0.1:39710"`。
- Qt `ChatController` 在 `sendMessage` 时携带当前皮肤 `manifest.expressions`。
- Qt `ChatController` 新增 `m_holdBuffer` / `flushHoldBuffer`：所有 `TEXT_MESSAGE_CONTENT` 通过 hold buffer 入 UI，当前实现为"立即 flush"，为 Phase 2.3 状态机预留位置。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider" --output-on-failure
```

## 当前限制

- ChatController 状态机仍是"流过即显示"，没有按 expression 切换 gate 文字。Phase 2.3 落地完整状态机和字符速率限制器。
- 配置只能通过环境变量；图形化设置面板和 Keychain 在 Phase 2.2。
- 会话历史不持久化；persona prompt 是最小版本。
- expression marker 解析放在 sidecar，Qt 仍然只接收已解析的事件。
- Provider 端口仍硬编码在 `DefaultListenAddr` / `ChatController.cpp`，未来如需动态端口由 sidecar 通过 stdout 上报。

## 后续入口

- Phase 2.2：设置 UI + API Key 安全存储。
- Phase 2.3：会话历史、完整 persona prompt、ChatController 状态机、字符速率限制器。
- Phase 2.4：Phased 动画与 `requestCleanFinishAndNotify` / `requestBoundaryAndNotify`。
