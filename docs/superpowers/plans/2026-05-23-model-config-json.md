# Model Config JSON Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 `providers.json` 替换 Keychain/QSettings 的模型配置存储，支持多组模型配置、可选 temperature/maxTokens、未配置时禁用聊天，并删除 Go mock provider。

**Architecture:** Qt 负责读取和保存 `providers.json`，启动 sidecar 时把当前配置写入环境变量。Go sidecar 只接收环境变量并调用 provider；没有配置时 provider 为 nil，聊天请求返回 `RUN_ERROR`。旧 SecretStore、MacSecretStore、mock provider 和相关测试检查一起删除。

**Tech Stack:** Qt 6/C++17/QML, Go 1.22, CMake/Ninja/CTest, Python contract checks.

---

## Scope

Included:

- 新增 `ProviderConfigFile`，读写 `<AppDataLocation>/providers.json`。
- `SettingsService` 改为使用 `ProviderConfigFile`。
- 设置页支持多组配置、新增、删除、切换、保存。
- `temperature` 和 `maxTokens` 可留空。
- `ChatController` 只注入当前有效配置；未配置时禁用输入框。
- Go 删除 mock provider；未配置时返回 `RUN_ERROR`。
- 更新 CMake、Python contract checks、C++ smoke tests、Go tests。

Excluded:

- 旧 QSettings/Keychain 自动迁移。
- 连接测试按钮。
- `/v1/config` 热更新接口。
- Windows Credential Manager / Linux Secret Service。

## File Map

Create:

- `apps/desktop/src/settings/ProviderConfigFile.h`
- `apps/desktop/src/settings/ProviderConfigFile.cpp`

Modify:

- `apps/desktop/src/settings/SettingsService.h`
- `apps/desktop/src/settings/SettingsService.cpp`
- `apps/desktop/src/settings/SettingsController.h`
- `apps/desktop/src/settings/SettingsController.cpp`
- `apps/desktop/qml/SettingsWindow.qml`
- `apps/desktop/qml/ChatWindow.qml`
- `apps/desktop/src/chat/ChatController.cpp`
- `apps/desktop/src/chat/ChatController.h`
- `apps/desktop/src/main.cpp`
- `apps/desktop/CMakeLists.txt`
- `apps/desktop/tests/settings_service_smoke.cpp`
- `apps/desktop/tests/chat_controller_smoke.cpp`
- `apps/agent-core/internal/chat/config/config.go`
- `apps/agent-core/internal/chat/config/config_test.go`
- `apps/agent-core/internal/chat/openai/provider.go`
- `apps/agent-core/internal/chat/openai/provider_test.go`
- `apps/agent-core/internal/chat/service/service.go`
- `apps/agent-core/internal/chat/service/service_test.go`
- `apps/agent-core/cmd/miles-agent/main.go`
- `apps/agent-core/cmd/miles-agent/main_test.go`
- `apps/agent-core/internal/api/server.go`
- `apps/agent-core/internal/api/server_test.go`
- `tests/check_phase_2_0_ai_chat_mvp.py`
- `tests/check_phase_2_1_provider.py`
- `tests/check_phase_2_2_settings.py`

Delete:

- `apps/desktop/src/settings/SecretStore.h`
- `apps/desktop/src/settings/SecretStore.cpp`
- `apps/desktop/src/settings/MacSecretStore.h`
- `apps/desktop/src/settings/MacSecretStore.mm`
- `apps/agent-core/internal/chat/mock_provider.go`

Keep:

- persona editing in `SettingsController` and `SettingsWindow.qml` stays.
- SQLite conversation history stays in Go.

---

## Task 0: Baseline

**Files:**
- Read-only

- [ ] **Step 1: Confirm branch**

Run:

```bash
git status --short --branch
```

Expected: branch is `feature/model-config-json`. Existing doc changes are allowed.

- [ ] **Step 2: Run current focused baseline**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
ctest --test-dir build -R "settings_service_smoke|chat_controller_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings" --output-on-failure
(cd apps/agent-core && go test ./...)
```

Expected: current baseline may pass before changes. If it fails, record the failure before editing.

- [ ] **Step 3: No commit**

This task changes nothing.

---

## Task 1: Replace Contract Checks First

**Files:**
- Modify: `tests/check_phase_2_0_ai_chat_mvp.py`
- Modify: `tests/check_phase_2_1_provider.py`
- Modify: `tests/check_phase_2_2_settings.py`

- [ ] **Step 1: Remove old mock/Keychain expectations**

Edit the checks so they stop requiring:

```text
apps/agent-core/internal/chat/mock_provider.go
mock-fallback
SecretStore
MacSecretStore
Security.framework
```

Add checks for:

```text
ProviderConfigFile
providers.json
QSaveFile
modelConfigs
activeModelConfig
std::optional<double>
std::optional<int>
provider == nil
RUN_ERROR
```

- [ ] **Step 2: Run contract checks and confirm red**

Run:

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
```

Expected: at least `check_phase_2_2_settings.py` fails because `ProviderConfigFile` does not exist yet. `check_phase_2_0_ai_chat_mvp.py` and `check_phase_2_1_provider.py` may also fail until mock removal is implemented.

- [ ] **Step 3: Commit contract update**

Run after confirming the expected failures:

```bash
git add tests/check_phase_2_0_ai_chat_mvp.py tests/check_phase_2_1_provider.py tests/check_phase_2_2_settings.py
git commit -m "test: update model config storage contract"
```

---

## Task 2: Add ProviderConfigFile

**Files:**
- Create: `apps/desktop/src/settings/ProviderConfigFile.h`
- Create: `apps/desktop/src/settings/ProviderConfigFile.cpp`
- Modify: `apps/desktop/tests/settings_service_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write failing ProviderConfigFile tests**

In `apps/desktop/tests/settings_service_smoke.cpp`, replace SecretStore-only setup with tests that cover:

```cpp
ProviderConfigFile file(tempFilePath);
assert(file.load() == ProviderConfigFile::LoadStatus::FileNotFound);
assert(file.configNames().isEmpty());

ProviderConfigFile::ModelConfig cfg;
cfg.name = QStringLiteral("deepseek");
cfg.baseUrl = QStringLiteral("https://api.deepseek.com");
cfg.apiKey = QStringLiteral("sk-test");
cfg.model = QStringLiteral("deepseek-chat");
cfg.temperature = 0.7;
cfg.maxTokens = std::nullopt;
file.setConfig(cfg.name, cfg);
file.setActiveModelConfig(cfg.name);
file.setMsPerChar(60);
assert(file.save());

ProviderConfigFile reopened(tempFilePath);
assert(reopened.load() == ProviderConfigFile::LoadStatus::Ok);
assert(reopened.activeModelConfig() == QStringLiteral("deepseek"));
assert(reopened.config(QStringLiteral("deepseek")).apiKey == QStringLiteral("sk-test"));
assert(reopened.config(QStringLiteral("deepseek")).temperature.has_value());
assert(!reopened.config(QStringLiteral("deepseek")).maxTokens.has_value());
assert(reopened.msPerChar() == 60);
```

Also add tests for:

```cpp
// invalid JSON becomes ParseError and creates providers.json.bak
// unknown fields survive load/save through extraFields
// deleting active config selects the first remaining config
// duplicate or empty names are rejected by setConfig()
```

- [ ] **Step 2: Run test and confirm red**

Run:

```bash
cmake --build build --target SettingsServiceSmoke
ctest --test-dir build -R "settings_service_smoke" --output-on-failure
```

Expected: build fails because `ProviderConfigFile` is not implemented.

- [ ] **Step 3: Implement ProviderConfigFile**

Add this public shape in `ProviderConfigFile.h`:

```cpp
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

class ProviderConfigFile
{
public:
    enum class LoadStatus { Ok, FileNotFound, ParseError, PermissionError };

    struct ModelConfig {
        QString name;
        QString baseUrl;
        QString apiKey;
        QString model;
        std::optional<double> temperature;
        std::optional<int> maxTokens;
        QJsonObject extraFields;
    };

    explicit ProviderConfigFile(QString path = {});

    LoadStatus load();
    bool save();

    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }

    QString activeModelConfig() const;
    void setActiveModelConfig(const QString &name);

    QStringList configNames() const;
    ModelConfig config(const QString &name) const;
    bool setConfig(const QString &name, const ModelConfig &cfg);
    void removeConfig(const QString &name);
    void moveConfig(int fromIndex, int toIndex);

    int msPerChar() const;
    void setMsPerChar(int value);

private:
    QString m_path;
    QString m_activeModelConfig;
    QList<ModelConfig> m_configs;
    int m_msPerChar = 80;
    QJsonObject m_rootExtraFields;
    QString m_lastError;
};
```

Implementation rules:

```text
Use QStandardPaths::AppDataLocation when path is empty.
Use QJsonDocument for parse/write.
Use QSaveFile for save.
Set file permissions before writing API key and after commit.
Use modelConfigs array order as UI order.
Keep extra root fields and per-config extra fields.
Clamp msPerChar to 40..200.
Reject empty duplicate config names.
Backup invalid JSON to providers.json.bak.
```

- [ ] **Step 4: Run test and confirm green**

Run:

```bash
cmake --build build --target SettingsServiceSmoke
ctest --test-dir build -R "settings_service_smoke" --output-on-failure
```

Expected: `settings_service_smoke` passes.

- [ ] **Step 5: Commit ProviderConfigFile**

Run:

```bash
git add apps/desktop/src/settings/ProviderConfigFile.h apps/desktop/src/settings/ProviderConfigFile.cpp apps/desktop/tests/settings_service_smoke.cpp apps/desktop/CMakeLists.txt
git commit -m "feat: add provider config json store"
```

---

## Task 3: Refactor Qt Settings Layer

**Files:**
- Modify: `apps/desktop/src/settings/SettingsService.h`
- Modify: `apps/desktop/src/settings/SettingsService.cpp`
- Modify: `apps/desktop/src/settings/SettingsController.h`
- Modify: `apps/desktop/src/settings/SettingsController.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/tests/settings_service_smoke.cpp`

- [ ] **Step 1: Write failing SettingsService tests**

Add tests for this behavior:

```cpp
SettingsService service(configPath);
assert(service.configNames().isEmpty());
assert(!service.providerConfigured());

SettingsService::ModelConfig cfg;
cfg.name = QStringLiteral("daily");
cfg.baseUrl = QStringLiteral("https://api.example.com");
cfg.apiKey = QStringLiteral("sk-json");
cfg.model = QStringLiteral("gpt-test");
cfg.temperature = std::nullopt;
cfg.maxTokens = 4096;
assert(service.setConfig(cfg.name, cfg));
service.setActiveModelConfig(cfg.name);
assert(service.save());

SettingsService reopened(configPath);
assert(reopened.providerConfigured());
assert(reopened.activeModelConfig() == QStringLiteral("daily"));
assert(reopened.baseUrl() == QStringLiteral("https://api.example.com"));
assert(reopened.apiKey() == QStringLiteral("sk-json"));
assert(reopened.model() == QStringLiteral("gpt-test"));
assert(!reopened.temperature().has_value());
assert(reopened.maxTokens().value() == 4096);
```

Add controller tests for:

```text
openWindow() loads actual API key into password field.
save() can add a new config.
save() rejects invalid temperature and maxTokens.
persona save still works.
```

- [ ] **Step 2: Run tests and confirm red**

Run:

```bash
cmake --build build --target SettingsServiceSmoke
ctest --test-dir build -R "settings_service_smoke" --output-on-failure
```

Expected: build fails because `SettingsService` still takes `SecretStore*`.

- [ ] **Step 3: Refactor SettingsService**

Change `SettingsService` to own `ProviderConfigFile`.

Required public API:

```cpp
struct ModelConfig {
    QString name;
    QString baseUrl;
    QString apiKey;
    QString model;
    std::optional<double> temperature;
    std::optional<int> maxTokens;
};

explicit SettingsService(QString configPath = {}, QObject *parent = nullptr);

QStringList configNames() const;
QString activeModelConfig() const;
void setActiveModelConfig(const QString &name);
ModelConfig config(const QString &name) const;
bool setConfig(const QString &name, const ModelConfig &cfg);
void removeConfig(const QString &name);

QString baseUrl() const;
QString apiKey() const;
QString model() const;
std::optional<double> temperature() const;
std::optional<int> maxTokens() const;
bool providerConfigured() const;
```

Keep:

```cpp
int msPerChar() const;
void setMsPerChar(int value);
bool save();
```

- [ ] **Step 4: Refactor SettingsController**

Expose QML-friendly string properties:

```text
configNames: QStringList
activeModelConfig: QString
configName: QString
baseUrl: QString
apiKey: QString
model: QString
temperatureText: QString
maxTokensText: QString
validationError: QString
providerConfigured: bool
```

Rules:

```text
temperatureText empty => std::nullopt
maxTokensText empty => std::nullopt
temperatureText must parse to 0..2
maxTokensText must parse to integer > 0
save() returns without closing window when validationError is set
```

- [ ] **Step 5: Update main.cpp**

Replace:

```cpp
std::unique_ptr<SecretStore> secretStore = SecretStore::create();
SettingsService settingsService(secretStore.get());
```

With:

```cpp
SettingsService settingsService;
```

- [ ] **Step 6: Run tests and confirm green**

Run:

```bash
cmake --build build --target SettingsServiceSmoke
ctest --test-dir build -R "settings_service_smoke" --output-on-failure
```

Expected: `settings_service_smoke` passes.

- [ ] **Step 7: Commit settings refactor**

Run:

```bash
git add apps/desktop/src/settings/SettingsService.h apps/desktop/src/settings/SettingsService.cpp apps/desktop/src/settings/SettingsController.h apps/desktop/src/settings/SettingsController.cpp apps/desktop/src/main.cpp apps/desktop/tests/settings_service_smoke.cpp
git commit -m "refactor: use json provider settings"
```

---

## Task 4: Update Settings UI and Chat UI

**Files:**
- Modify: `apps/desktop/qml/SettingsWindow.qml`
- Modify: `apps/desktop/qml/ChatWindow.qml`
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write failing ChatController/UI smoke tests**

Add assertions in `chat_controller_smoke.cpp`:

```cpp
SettingsService unconfiguredSettings(tempPath);
ChatController unconfigured(&runtime, &unconfiguredSettings);
unconfigured.startSidecar();
assert(!unconfigured.providerConfigured());

SettingsService configuredSettings(tempPath);
SettingsService::ModelConfig cfg;
cfg.name = QStringLiteral("daily");
cfg.baseUrl = QStringLiteral("https://api.example.com");
cfg.apiKey = QStringLiteral("sk-test");
cfg.model = QStringLiteral("gpt-test");
cfg.temperature = std::nullopt;
cfg.maxTokens = std::nullopt;
configuredSettings.setConfig(cfg.name, cfg);
configuredSettings.setActiveModelConfig(cfg.name);
configuredSettings.save();

ChatController configured(&runtime, &configuredSettings);
configured.startSidecar();
assert(configured.providerConfigured());
```

Add static contract checks for QML:

```text
SettingsWindow.qml contains config selector, new/delete buttons, temperatureText, maxTokensText.
ChatWindow.qml input enabled condition includes providerConfigured.
ChatWindow.qml placeholder contains 未配置模型 or 先点设置填写模型配置.
```

- [ ] **Step 2: Run tests and confirm red**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build -R "chat_controller_smoke|check_phase_2_2_settings" --output-on-failure
```

Expected: failures show old single-config UI and old `providerConfigured` behavior.

- [ ] **Step 3: Update SettingsWindow.qml**

Required UI:

```text
Top row: ComboBox for configNames, + button, delete button.
Form fields: name, baseUrl, apiKey, model, temperatureText, maxTokensText.
Temperature and maxTokens use TextField, not Slider/SpinBox.
Validation error label shows SettingsController.validationError.
Persona editor remains below provider fields.
Save button calls SettingsController.save().
Cancel calls SettingsController.revert().
Open config folder button calls SettingsController.openConfigDirectory().
```

- [ ] **Step 4: Update ChatWindow.qml**

Change input enabled rule to:

```qml
enabled: App.ChatController.sidecarReady
         && App.ChatController.providerConfigured
         && !App.ChatController.sending
```

Placeholder rule:

```qml
placeholderText: !App.ChatController.providerConfigured
        ? "先点设置填写模型配置"
        : (disconnectedInput ? "未连接，点重连或稍后重试" : "输入消息")
```

- [ ] **Step 5: Update ChatController env injection**

Rules:

```text
Set providerConfigured only when baseUrl/apiKey/model are all non-empty.
Insert MILES_PROVIDER_TEMPERATURE only when temperature has value.
Insert MILES_PROVIDER_MAX_TOKENS only when maxTokens has value.
When sidecar is healthy but providerConfigured is false, statusText is 未配置模型.
```

- [ ] **Step 6: Run UI/controller tests**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build -R "chat_controller_smoke|check_phase_2_2_settings" --output-on-failure
```

Expected: both pass.

- [ ] **Step 7: Commit UI/controller work**

Run:

```bash
git add apps/desktop/qml/SettingsWindow.qml apps/desktop/qml/ChatWindow.qml apps/desktop/src/chat/ChatController.h apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/chat_controller_smoke.cpp tests/check_phase_2_2_settings.py
git commit -m "feat: support multiple provider configs in UI"
```

---

## Task 5: Remove SecretStore Build Path

**Files:**
- Delete: `apps/desktop/src/settings/SecretStore.h`
- Delete: `apps/desktop/src/settings/SecretStore.cpp`
- Delete: `apps/desktop/src/settings/MacSecretStore.h`
- Delete: `apps/desktop/src/settings/MacSecretStore.mm`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `tests/check_phase_2_2_settings.py`

- [ ] **Step 1: Write failing absence checks**

In `tests/check_phase_2_2_settings.py`, require these strings are absent from CMake and main:

```text
SecretStore
MacSecretStore
Security.framework
MILES_HAS_MAC_SECRET_STORE
```

- [ ] **Step 2: Run and confirm red**

Run:

```bash
python3 tests/check_phase_2_2_settings.py
```

Expected: fails while old files/CMake references remain.

- [ ] **Step 3: Delete old files and CMake entries**

Remove:

```text
src/settings/SecretStore.cpp
src/settings/SecretStore.h
src/settings/MacSecretStore.h
src/settings/MacSecretStore.mm
target_link_libraries(... "-framework Security")
target_compile_definitions(... MILES_HAS_MAC_SECRET_STORE)
```

- [ ] **Step 4: Run checks**

Run:

```bash
python3 tests/check_phase_2_2_settings.py
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
```

Expected: check passes and desktop app builds.

- [ ] **Step 5: Commit deletion**

Run:

```bash
git add apps/desktop/CMakeLists.txt apps/desktop/src/main.cpp tests/check_phase_2_2_settings.py
git rm apps/desktop/src/settings/SecretStore.h apps/desktop/src/settings/SecretStore.cpp apps/desktop/src/settings/MacSecretStore.h apps/desktop/src/settings/MacSecretStore.mm
git commit -m "chore: remove keychain settings store"
```

---

## Task 6: Go Optional Fields and No Mock Provider

**Files:**
- Delete: `apps/agent-core/internal/chat/mock_provider.go`
- Modify: `apps/agent-core/internal/chat/config/config.go`
- Modify: `apps/agent-core/internal/chat/config/config_test.go`
- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Modify: `apps/agent-core/internal/chat/service/service.go`
- Modify: `apps/agent-core/internal/chat/service/service_test.go`
- Modify: `apps/agent-core/internal/api/server.go`
- Modify: `apps/agent-core/internal/api/server_test.go`
- Modify: `apps/agent-core/cmd/miles-agent/main.go`
- Modify: `apps/agent-core/cmd/miles-agent/main_test.go`

- [ ] **Step 1: Write failing Go tests**

Add config tests:

```go
func TestFromEnvLeavesOptionalFieldsNilWhenUnset(t *testing.T) {
    t.Setenv("MILES_PROVIDER_BASE_URL", "https://api.example.com")
    t.Setenv("MILES_PROVIDER_API_KEY", "sk-test")
    t.Setenv("MILES_PROVIDER_MODEL", "gpt-test")
    t.Setenv("MILES_PROVIDER_TEMPERATURE", "")
    t.Setenv("MILES_PROVIDER_MAX_TOKENS", "")

    cfg := FromEnv()
    if cfg.Temperature != nil {
        t.Fatalf("temperature = %v, want nil", *cfg.Temperature)
    }
    if cfg.MaxTokens != nil {
        t.Fatalf("maxTokens = %v, want nil", *cfg.MaxTokens)
    }
}
```

Add provider request tests:

```go
func TestStreamChatOmitsUnsetOptionalFields(t *testing.T) {
    // httptest server decodes request body and asserts:
    // _, hasTemperature := body["temperature"]
    // _, hasMaxTokens := body["max_tokens"]
    // both must be false when provider fields are nil.
}
```

Add nil-provider API test:

```go
func TestChatMessagesReturnsRunErrorWhenProviderMissing(t *testing.T) {
    // Create store and conversation.
    // Construct chatservice.New(store, nil, catalog, "").
    // POST /v1/chat/messages.
    // Assert SSE contains RUN_ERROR and 未配置模型.
}
```

- [ ] **Step 2: Run Go tests and confirm red**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: compile/test failures because config/provider still use concrete values and mock provider exists.

- [ ] **Step 3: Update config and provider**

Change:

```go
type ProviderConfig struct {
    BaseURL     string
    APIKey      string
    Model       string
    Temperature *float64
    MaxTokens   *int
}
```

Rules:

```text
Unset env var => nil.
Invalid optional env var => nil.
Enabled() still requires baseUrl/apiKey/model.
openai request uses pointer fields with omitempty.
```

- [ ] **Step 4: Add nil-provider handling**

In ChatService or API handler:

```go
if s.provider == nil {
    events := make(chan chat.StreamEvent, 1)
    go func() {
        defer close(events)
        events <- chat.StreamEvent{
            Type:  "RUN_ERROR",
            RunID: req.RunID,
            Error: "未配置模型，请在设置中填写 API Key 和模型信息",
        }
    }()
    return events, nil
}
```

- [ ] **Step 5: Delete mock provider**

Remove file and main fallback:

```text
chat.NewMockProvider(...)
provider label "mock-fallback"
```

When config is missing:

```go
var provider chat.Provider
label := "unconfigured"
```

- [ ] **Step 6: Run Go tests**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: all Go tests pass.

- [ ] **Step 7: Commit Go changes**

Run:

```bash
git add apps/agent-core
git rm apps/agent-core/internal/chat/mock_provider.go
git commit -m "feat: remove mock provider fallback"
```

---

## Task 7: Final Contracts and Docs

**Files:**
- Modify: `docs/v2/设计方案/模型配置与密钥存储设计.md`
- Modify: `docs/v2/设计方案/总体架构设计.md`
- Modify: `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`
- Modify: `docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md`
- Modify: `docs/v2/文档索引.md`

- [ ] **Step 1: Check docs match final implementation**

Run:

```bash
rg -n "SecretStore|MacSecretStore|mock-fallback|Keychain 已落地|不落盘" docs/v2/设计方案 docs/v2/阶段记录 docs/v2/文档索引.md
```

Expected: only historical notes remain, and each historical note points to `模型配置与密钥存储设计.md`.

- [ ] **Step 2: Run static contract checks**

Run:

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
python3 tests/check_phase_2_3_2_session_persona.py
```

Expected: all pass.

- [ ] **Step 3: Run focused build/test suite**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "settings_service_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|chat_text_pacer_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|check_phase_2_3_2_session_persona" --output-on-failure
(cd apps/agent-core && go test ./...)
git diff --check
```

Expected: all pass.

- [ ] **Step 4: Commit docs/test polish**

Run:

```bash
git add docs/v2 tests CMakeLists.txt apps/desktop/CMakeLists.txt
git commit -m "docs: align model config json plan"
```

Skip this commit if there are no remaining uncommitted docs/test changes.

---

## Task 8: Push and Open PR

**Files:**
- Git only

- [ ] **Step 1: Confirm final status**

Run:

```bash
git status --short --branch
git log --oneline -5
```

Expected: branch is `feature/model-config-json`; working tree is clean.

- [ ] **Step 2: Push branch**

Run:

```bash
git push -u origin feature/model-config-json
```

Expected: push succeeds.

- [ ] **Step 3: Create draft PR**

Use GitHub tooling with:

```text
Title: Replace Keychain settings with providers.json
Base: main
Head: feature/model-config-json
Draft: true
```

PR body:

```markdown
## Summary
- replace SecretStore/Keychain settings with providers.json
- support multiple provider configs and optional temperature/maxTokens
- remove mock provider fallback and return RUN_ERROR when model config is missing

## Verification
- python3 tests/check_phase_2_0_ai_chat_mvp.py
- python3 tests/check_phase_2_1_provider.py
- python3 tests/check_phase_2_2_settings.py
- python3 tests/check_phase_2_3_2_session_persona.py
- ctest --test-dir build -R "settings_service_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|chat_text_pacer_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|check_phase_2_3_2_session_persona" --output-on-failure
- (cd apps/agent-core && go test ./...)
- git diff --check
```

Expected: draft PR URL is created.

---

## Self-Review

- Spec coverage: all requirements from `docs/v2/设计方案/模型配置与密钥存储设计.md` are mapped to tasks.
- No branch name uses `codex`.
- TDD order is explicit: write failing test, run red, implement, run green.
- No migration is planned because the project has not shipped.
- Old docs stay as history but must clearly point to the new design.
