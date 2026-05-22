# Phase 2.2 用户配置与安全存储

本文记录 Phase 2.2 的实现范围、验收方式和当前限制。Phase 2.2 将 Phase 2.1 的环境变量临时配置升级为桌面端设置入口，并把 API key 路由到系统密钥存储。

参考设计：`docs/v2/设计方案/AI 聊天动画编排设计.md` §7.2 与 §11 Phase 2.2 行。

## 完成范围

- 新增 `SecretStore` 抽象：提供 `available()`、`read()`、`write()`、`remove()` 和 `create()` 工厂。
- 新增 `MacSecretStore`：macOS 下使用 Security framework generic-password keychain，覆盖 `SecItemAdd`、`SecItemCopyMatching`、`SecItemDelete`。
- 新增 `NullSecretStore`：非 macOS 或密钥存储不可用时不把 API key 写入磁盘；环境变量仍可作为兜底来源。
- 新增 `SettingsService`：非 secret 字段通过 `QSettings` 持久化，字段包括 `provider/baseUrl`、`provider/model`、`provider/temperature`、`provider/maxTokens`、`chat/msPerChar`；API key 只走 `SecretStore`。
- 新增 `SettingsController` QML singleton：维护设置窗口的 staged value，支持 Save / Cancel。
- 新增 `SettingsWindow.qml`：提供 base URL、API key、model、temperature、max tokens、`msPerChar` 设置入口；API key 输入框使用 password echo mode。
- `ChatWindow.qml` 顶部增加“设置”按钮，打开设置窗口。
- `ChatWindow.qml` 支持选择聊天气泡文本；未连接时输入框显示明确的设置指引和醒目禁用态。
- `main.cpp` 设置 `QCoreApplication` organization / domain / application name，创建 `SettingsService` 与 `SettingsController`，加载 `SettingsWindow`。
- `ChatController::startSidecar()` 从 `SettingsService` 读取当前配置，将 `MILES_PROVIDER_BASE_URL` / `MILES_PROVIDER_API_KEY` / `MILES_PROVIDER_MODEL` / `MILES_PROVIDER_TEMPERATURE` / `MILES_PROVIDER_MAX_TOKENS` 注入 sidecar 的 `QProcessEnvironment`。
- 设置保存后触发 `ChatController::restartSidecar()`，让 Go sidecar 用新的 provider 配置启动。

## 安全边界

- API key 不写入 `QSettings`，也不进入仓库、日志或 QML 明文持久化文件。
- Qt 到 Go sidecar 的聊天 HTTP 请求不携带 API key；key 只通过子进程环境变量传给 sidecar。
- 如果系统密钥存储不可用，本阶段选择“不落盘”兜底：用户仍可临时填写并在当前进程内使用，但重启后需要重新输入，或继续使用外部环境变量。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "settings_service_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|pet_runtime_smoke|skin_manifest_loader_smoke" --output-on-failure
```

## 当前限制

- `msPerChar` 已持久化并暴露到设置 UI，但字符速率限制器在 Phase 2.3 消费。
- 设置保存采用重启 sidecar 的方式生效；没有引入 `/v1/config` 热更新接口。
- Windows Credential Manager 与 Linux Secret Service 尚未实现。
- 没有“测试连接”按钮；用户保存后通过发送聊天消息验证 provider 配置。
- Langfuse 可观测性不在 Phase 2.2 激活，留到 Phase 2.3 与会话、persona、调用链观测一起做。

## 后续入口

- Phase 2.3：会话历史、完整 persona prompt、ChatController 动画-文字同步状态机、字符速率限制器、Langfuse 可观测性接入。
- Phase 2.4：Phased 动画、动画链与 clean finish / boundary 运行时接口。
