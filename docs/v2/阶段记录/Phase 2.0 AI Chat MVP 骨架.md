# Phase 2.0 AI Chat MVP 骨架

本文记录 Phase 2.0 的实现范围、验收方式和后续限制。Phase 2.0 只打通 AI 聊天桌宠的最小闭环，不接真实模型 provider，也不引入 tools、MCP、权限或长期记忆。

## 完成范围

- 新增 `apps/agent-core` Go sidecar，提供 `/health` 和 `POST /v1/chat/messages`。
- `POST /v1/chat/messages` 使用 `text/event-stream` 返回 AG-UI 风格事件。
- mock provider 能稳定输出 `RUN_STARTED`、文本 token、`RUN_FINISHED` 和 `CUSTOM miles.pet.expression.requested`。
- Qt 侧新增 `ChatStreamEventParser`，支持拆包 SSE、JSON envelope 和自定义 expression 事件。
- Qt 侧新增 `ChatController`，负责启动本地 sidecar、健康检查、发送消息、取消流式回复、维护聊天消息模型。
- 桌面应用新增 QML `ChatWindow`，通过右键菜单“聊天”打开。
- 聊天流式状态通过 `PetRuntime::requestExpression(state, expression)` 进入现有皮肤表达映射。
- CMake 能在构建桌面应用时构建并复制 `miles-agent` sidecar。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
(cd apps/agent-core && go test ./...)
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "chat_stream_event_parser_smoke|chat_controller_smoke|check_phase_2_0_ai_chat_mvp" --output-on-failure
```

## 当前限制

- 只有 mock provider，不读取 API key，不访问外部模型服务。
- ChatWindow 是独立普通窗口；桌宠本体仍保持原生 QWidget `PetSurfaceWindow`。
- 会话不持久化，重启后不保留历史。
- expression 只覆盖 Phase 2.0 的 thinking、speaking、idle、error 最小状态链路。
- Go 已成为当前根 CMake 构建的前置条件。

## 后续入口

- Phase 2.1：接入 OpenAI-compatible provider。
- Phase 2.2：补用户配置和安全存储。
- Phase 2.3：补会话历史和人设。
- Phase 2.4：打磨 expression 与回复体验。
