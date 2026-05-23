# Phase 2.3.3 Langfuse 可观测性

本文记录 Phase 2.3.3 的实现范围、验收方式和已知限制。Phase 2.3.3 激活 Go sidecar provider 层的观测 hook，通过 OpenTelemetry OTLP 向 Langfuse 写入 LLM 调用 trace；Qt 侧负责把 Langfuse 配置写入 `settings.json` 并在启动 sidecar 时注入环境变量。

## 实现范围

- `settings.json` 新增顶层 `langfuse` 对象：`enabled`、`host`、`publicKey`、`secretKey`、`captureContent`。
- Qt 设置窗口新增 `其他设置` tab。文字节奏从角色人格页移到该 tab，Langfuse Host / Public Key / Secret Key / 内容记录开关也在该 tab 配置。
- `ProviderConfigFile`、`SettingsService`、`SettingsController` 读写 Langfuse 配置，并继续保留未知字段，避免旧版本覆盖未来字段。
- `ChatController::launchSidecarProcess()` 在 Langfuse 启用且 host/public key/secret key 都完整时注入：
  - `MILES_LANGFUSE_ENABLED=1`
  - `LANGFUSE_HOST`
  - `LANGFUSE_PUBLIC_KEY`
  - `LANGFUSE_SECRET_KEY`
  - `MILES_LANGFUSE_CAPTURE_CONTENT`
- Go sidecar 新增 `config.LangfuseConfig`，未启用或缺少任一必填项时不激活 tracing。
- Go provider 通过 `observability.WrapProvider` 包装，覆盖 `StreamChat` 和 `Complete`。聊天请求标记为 `operation=chat`，摘要请求标记为 `operation=summary`。
- Langfuse 接入使用官方 OTLP traces endpoint：`/api/public/otel/v1/traces`，认证方式为 Basic auth，写入 `x-langfuse-ingestion-version: 4`。

## Trace 内容

每次 provider 调用写入一个 Langfuse generation observation：

- session：使用 `conversationId` 写入 `langfuse.session.id`。
- 模型：写入 `langfuse.observation.model.name` 和 temperature/maxTokens 参数。
- 请求类型：写入 `operation=chat` 或 `operation=summary`。
- prompt 版本：写入 system prompt hash，而不是把 persona prompt 作为版本系统持久化。
- usage：当前用字符数估算 token，写入 `langfuse.observation.usage_details`，并用 `usage_source=estimated` 标明来源。
- 内容：`captureContent=true` 时记录 prompt/messages/output；`false` 时只记录输入/输出字符数和元数据。
- 错误：provider 返回错误或流式 `RUN_ERROR` 时，span 标记 error，并写入 Langfuse observation level/status message。

## 验收

自动检查：

```bash
python3 tests/check_phase_2_3_3_langfuse.py
cd apps/agent-core && go test ./...
ctest --test-dir build -R "check_phase_2_3_3_langfuse|settings_service_smoke|chat_controller_smoke" --output-on-failure
```

手动检查：

1. 打开设置窗口，确认有 `其他设置` tab。
2. 在 `其他设置` 中调整文字节奏并保存，确认聊天输出节奏随设置变化。
3. 填写 Langfuse Host / Public Key / Secret Key，启用 Langfuse 后保存。
4. 发送一条聊天消息，确认 Langfuse 中出现带 `session.id`、model、usage details 和 generation observation 的 trace。
5. 关闭 `记录内容` 后再发送消息，确认 trace 只保留元数据和字符数，不包含 prompt/output 正文。

## 已知限制

- token usage 目前是估算值。OpenAI-compatible streaming 响应如果后续提供真实 usage，可在 provider 层替换估算。
- cost 依赖 Langfuse 根据 model/usage 做后处理，本阶段不在客户端计算价格。
- `secretKey` 按模型配置设计写入明文 `settings.json`；日志只输出是否配置，不输出密钥值。
- Langfuse 初始化失败只记录 warning，不阻止聊天主流程。
