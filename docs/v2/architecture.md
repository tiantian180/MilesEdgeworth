# MilesEdgeworth v2 架构设计

本文档记录 MilesEdgeworth v2 的总体技术路线、模块边界、阶段目标和长期扩展方向。
v2 的第一目标不是立刻做完整 agent 平台，而是先做出一个有旧版手感、可配置大模型聊天的 AI 桌宠。

## 1. 设计目标

v2 要同时满足两件事：

1. 保留旧版 MilesEdgeworth 的桌宠体验：透明异形窗口、置顶、拖拽、随机动作、走路/跑步、检察官徽章、音效、喝茶、睡觉等。
2. 建立新的 AI 桌宠架构：聊天窗口、设置中心、OpenAI-compatible provider、人设 prompt、流式回复、状态驱动动画，并为后续 tools、skills、MCP、权限确认和插件系统预留边界。

v2 不迁移旧版的图片置顶查看器功能。`PicViewer` 只作为 legacy 代码保留，不进入新版主线。

## 2. 技术路线

推荐路线：

```text
Qt 6 + Qt Quick/QML + 少量 C++ + Go sidecar Agent Core
```

职责划分：

- Qt/QML 负责桌宠身体、窗口壳层和用户界面。
- C++ 负责 Qt 与操作系统相关的能力，例如透明窗口、置顶、跨 Space、托盘、拉起 sidecar。
- Go 负责模型 API、流式回复、配置、历史记录、存储、安全 key 管理，以及未来 agent runtime。

这样做的原因是：

- Qt 在 Windows、macOS、Linux 桌面窗口能力上更可靠，适合桌宠。
- QML 比 Qt Widgets 更适合动画、状态切换、气泡和自定义 UI。
- Go 更适合承接 provider、SQLite、工具系统、MCP、插件和未来 agent harness。
- Qt 侧不应该承担大量 AI/Agent 业务逻辑，否则长期维护会变重。

## 3. 仓库组织

v2 在当前仓库的新分支中开发，先新增 v2 目录，不急着移动旧版代码，避免一开始产生巨大 diff。

建议结构：

```text
MilesEdgeworth/
  apps/
    desktop/
      CMakeLists.txt
      src/
        main.cpp
        app/
        platform/
        pet/
        bridge/
      qml/
        PetWindow.qml
        ChatWindow.qml
        DashboardWindow.qml
        components/
      resources/

    agent-core/
      go.mod
      cmd/miles-agent/
      internal/
        api/
        chat/
        provider/
        persona/
        storage/
        config/
        pet/
        runtime/

  assets/
    skins/
      miles-edgeworth/
        manifest.json
        behavior.json
        animations/
        sounds/
        icons/

  schemas/
    skin-manifest.schema.json
    behavior-profile.schema.json
    pet-event.schema.json

  docs/
    v2/
      architecture.md
      pet-runtime-animation-design.md
      animation-taxonomy.md
      provider-and-chat-design.md
      packaging.md
```

旧版根目录文件先保留。等 v2 主线稳定后，再考虑把旧版 Qt Widgets 工程整体归档到 `legacy/`。

## 4. Qt Desktop 职责

`apps/desktop` 是桌面端应用本体。

主要职责：

- 创建透明、无边框、置顶、可拖拽的桌宠窗口。
- 支持 macOS 跨 Space / 全屏层级、Windows topmost、Linux 窗口管理器 best-effort。
- 管理系统托盘、右键菜单、ChatWindow、DashboardWindow。
- 播放桌宠动画、音效、气泡、检察官徽章等表现层效果。
- 实现 Pet Runtime / Animation Orchestrator。
- 通过 Bridge 与 Go sidecar 通信。

Qt 侧不直接关心：

- 具体 provider 是 OpenAI、DeepSeek、Moonshot 还是 OpenRouter。
- API key 如何调用模型。
- Chat Completions、Responses API 或 Eino ADK 的具体差异。
- SQLite 表结构和聊天历史持久化细节。

Qt 只接收统一事件，并把与桌宠相关的语义事件交给 Pet Runtime。

## 5. Go Agent Core 职责

`apps/agent-core` 是本地 sidecar 后端。

主要职责：

- 管理 provider 配置：`base_url`、`api_key`、`model`、`temperature`、`max_tokens` 等。
- 管理 Miles 人设 prompt 和会话上下文。
- 调用 OpenAI-compatible `/v1/chat/completions`，并预留 OpenAI `/v1/responses`。
- 后续把 Eino ADK 作为可插拔 Agent Runtime 候选，而不是 MVP 主路径。
- 保存聊天会话、历史记录、普通配置和日志。
- API key 优先保存到系统安全存储：macOS Keychain、Windows Credential Manager、Linux Secret Service/libsecret。
- 对 Qt 输出统一的事件流，不暴露 provider 差异。
- 后续扩展 tools、skills、MCP、权限确认、插件和脚本执行。

第一版不做完整 agent harness。Go Core 先专注于“可配置模型的人设聊天”。

## 6. 通信协议

Qt 与 Go 之间使用本地通信：

```text
传输层：HTTP + SSE
协议语义：AG-UI-compatible event stream
桌宠扩展：CUSTOM + miles.pet.* namespace
```

普通请求使用 HTTP：

- `GET /v1/config`
- `PUT /v1/config`
- `GET /v1/chat/sessions`
- `POST /v1/chat/messages`

流式输出使用 SSE。标准聊天事件尽量贴近 AG-UI：

- `RUN_STARTED`
- `TEXT_MESSAGE_START`
- `TEXT_MESSAGE_CONTENT`
- `TEXT_MESSAGE_END`
- `RUN_FINISHED`
- `RUN_ERROR`
- `TOOL_CALL_*`，后续阶段使用

桌宠专属事件使用 `CUSTOM`：

```json
{
  "type": "CUSTOM",
  "name": "miles.pet.expression.requested",
  "value": {
    "state": "speaking",
    "expression": "objection",
    "bubble": "异议！这个推论还有漏洞。"
  }
}
```

建议的 Miles 扩展事件：

- `miles.pet.state.changed`
- `miles.pet.expression.requested`
- `miles.pet.bubble.show`
- `miles.pet.bubble.hide`
- `miles.pet.motion.requested`
- `miles.pet.sound.requested`
- `miles.pet.permission.requested`，后续阶段
- `miles.pet.tool.status.changed`，后续阶段

Go 可以输出语义事件，但不能直接指定具体 GIF。具体动画选择归 Qt Pet Runtime。

## 7. 模型接口策略

第一版优先支持 OpenAI-compatible Chat Completions：

```text
base_url + api_key + model + /v1/chat/completions
```

这是为了兼容更多中国用户常用 provider，例如 DeepSeek、Moonshot、硅基流动、OpenRouter 和本地兼容服务。

OpenAI 官方 provider 可以预留 Responses API 增强。Responses API 更适合长期的工具调用、多模态和 agentic workflow，但很多 OpenAI-compatible provider 并不支持 `/v1/responses`，所以不应作为 MVP 唯一路径。

Eino ADK 值得在后续 agent 阶段评估。它可以承接工具调用、MCP、多 agent 和 callback/trace，但 MVP 不应被 Eino 深度绑定。

建议 Go Core 内部保留这些抽象：

```text
ChatService
ProviderAdapter
ChatCompletionsAdapter
ResponsesAdapter
Future: EinoAgentRuntime
ConversationStore
PetEventEmitter
```

## 8. 配置与存储

MVP 存储策略：

- SQLite：聊天历史、会话、普通配置、皮肤选择、日志索引。
- 系统安全存储：API key。
- 本地开发可允许临时配置兜底，但 release 应默认避免明文保存 key。

Dashboard 修改配置时：

```text
Dashboard
=> PUT /v1/config
=> Go Core 保存 SQLite / Keychain
=> SSE: config.updated
=> Qt 刷新 UI 或提示重新连接
```

日志和配置导出必须脱敏，不允许把 API key 打进普通日志。

## 9. 皮肤与行为配置

皮肤系统拆成两个文件：

- `manifest.json`：描述这个皮肤有哪些素材、动作、表达标签、朝向变体和 fallback。
- `behavior.json`：描述这个角色平时怎么动，例如随机动作概率、点击区域映射、右键菜单动作映射。

这样做可以把“素材能力”和“角色行为”分开。

模型不应该直接生成固定项目内置的情绪词，也不应该直接点名 GIF 文件。可用表达标签由当前皮肤 manifest 动态声明：

```text
固定系统状态：idle / thinking / speaking / moving / error / waiting_permission
动态表达标签：由 skin manifest 定义，例如 objection / polite / tea_break / confident
具体动画：由 Qt Pet Runtime 根据 manifest 映射
```

皮肤必须提供 `neutral` 或等价兜底。模型输出未知标签时，Go Core 或 Qt Runtime 应降级到兜底表达，而不是报错。

## 10. Pet Runtime 概览

Pet Runtime 是 Qt 侧的核心。它统一处理：

- 空闲随机动画。
- 走路/跑步/目标移动。
- 单击、双击、拖拽晃动。
- 右键菜单触发动作。
- 聊天状态：thinking / speaking / error。
- Agent 状态：工具执行、权限请求、任务完成。
- 气泡、音效、检察官徽章等副作用。

所有来源都发 `ActionRequest`，不直接播放动画文件。Pet Runtime 根据当前状态、优先级、切换规则、皮肤能力和朝向选择最终动画。

详细方案见 `docs/v2/pet-runtime-animation-design.md`。

## 11. 阶段计划

### Phase 0：桌宠壳验证

- Qt 6 / QML 显示 GIF、WebP、APNG 的可行性。
- 透明、无边框、置顶、跨 Space。
- 像素级可点击区域或 mask。
- 拖拽、托盘、右键菜单。
- 多窗口入口：ChatWindow、DashboardWindow。
- macOS 优先，但代码结构保留 Windows/Linux 平台分支。

### Phase 1：旧版手感还原

- Pet Runtime 最小版。
- `currentAction + pendingRequest` 调度模型。
- `loop / oneshot` 播放。
- 左右朝向和 8 方向移动动画。
- 随机待机、走路、跑步。
- 单击、双击、拖拽晃动。
- 检察官徽章、音效、喝茶、睡觉。
- 图片置顶查看器不迁移。

### Phase 2：AI 聊天闭环

- Go sidecar。
- HTTP + SSE。
- OpenAI-compatible provider 配置。
- Dashboard 基础设置。
- ChatWindow 流式聊天。
- 聊天历史保存。
- Miles 人设 prompt。
- `thinking / speaking / idle / error` 状态驱动桌宠。
- 动态 expression tags 和气泡。

### Phase 3：高级动画与皮肤系统

- `phased` 动画：enter / loop / exit。
- 单 GIF 局部帧段循环。
- manifest schema 校验。
- 皮肤预览和校准工具。
- 更完整的 fallback 规则。

### Phase 4+：Agent 能力

- tools、skills、MCP。
- 权限确认。
- ToolRegistry、SkillManager、MCPManager。
- 插件 manifest。
- 日志、trace、任务进度。
- 评估 Eino ADK 作为 Agent Runtime。

## 12. 文档维护规则

v2 设计文档是活文档。

- 行为规则变化时，更新对应设计文档。
- manifest 字段变化时，更新 schema 和示例。
- 发现旧版特殊逻辑时，补充到 v1 parity checklist。
- 实现中推翻设计时，必须在 commit 或 PR 描述里说明原因。
- 代码注释中文为主，重点解释模块职责、跨平台注意事项、调度规则和不直观的实现原因。
