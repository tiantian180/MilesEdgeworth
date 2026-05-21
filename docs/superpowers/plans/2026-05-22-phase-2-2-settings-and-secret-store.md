# Phase 2.2 用户配置与安全存储 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the env-var-only provider config with a persisted user settings store backed by the OS keychain (macOS for now), expose it through a new QML `SettingsWindow`, and wire `ChatController` to read the persisted values and inject them into the sidecar via `QProcessEnvironment` (no API key on disk and no API key over HTTP).

**Architecture:** Three new desktop modules — (1) `SecretStore` is a small C++ interface with a real macOS implementation in Obj-C++ (Security framework) and a `NullSecretStore` fallback; (2) `SettingsService` is a `QObject` that persists non-secret fields through `QSettings` and routes the API key through `SecretStore`; (3) `SettingsController` is the QML-singleton facade for the new `SettingsWindow.qml`. `ChatController::startSidecar()` now reads the live values out of `SettingsService` and inserts `MILES_PROVIDER_*` into the `QProcess` env before launching `miles-agent`. After a successful save, `SettingsController` emits `saved()`, `ChatController` kills the running sidecar and starts a fresh one. The Go sidecar code does **not** change — it continues to read env vars through the existing `internal/chat/config` package.

**Tech Stack:** Qt 6 (C++17 + Obj-C++ for macOS Security framework), CMake/Ninja/CTest, `QSettings`/`QProcessEnvironment`/`QProcess`, QML/QtQuick.Controls.Basic for the settings UI. Python 3 for the static contract check. No new third-party dependencies (Apple Security framework is part of the SDK).

---

## Scope Check

This plan implements **Phase 2.2: 用户配置与安全存储** only. It is a single subsystem (desktop-side settings + key storage) — no sub-plan split needed.

Included:
- `SecretStore` abstract interface, `NullSecretStore` fallback, `MacSecretStore` macOS implementation.
- `SettingsService` (non-QObject persistence service taking an injected `SecretStore*` for testability) + `SettingsController` QML singleton.
- New `apps/desktop/qml/SettingsWindow.qml` with provider, base URL, API key (masked), model, temperature, max tokens, msPerChar fields + Save / Cancel.
- An entry-point to open the settings window from `ChatWindow.qml`.
- `ChatController::startSidecar()` reads settings, injects `MILES_PROVIDER_*` env into the QProcess.
- `ChatController` listens for `SettingsController::saved()` and restarts the sidecar.
- `QCoreApplication` org/app names set in `main.cpp` so `QSettings` lands at the expected macOS plist.
- `msPerChar` field is persisted but **not yet consumed** — Phase 2.3's character rate limiter reads it.
- Tests: `SettingsService` smoke test using `InMemorySecretStore`, ChatController-side smoke test asserting QProcess env was populated, Python contract check, doc updates.

Excluded:
- Windows / Linux secret store implementations (the `SecretStore::create()` factory returns `NullSecretStore` everywhere except macOS; the env-var fallback path covers those platforms).
- Live consumption of `msPerChar` — the limiter itself is Phase 2.3.
- A live "Test connection" button — out of scope; user verifies by sending a chat message after Save.
- Multi-profile / multi-provider switcher UI — Phase 3+ when MCP / multiple providers land.
- In-process hot-reload of provider config (the sidecar is restarted; HTTP `/v1/config` is not introduced).
- Langfuse activation — the middleware hook reservation belongs to Phase 2.3.

## Assumptions

- Execution starts from `/Users/tian/projects/my-projects/MilesEdgeworth` on `main` (or a worktree from the branch you're currently on, e.g. `phase-2-1-provider` if continuing).
- Phase 2.1 is shipped: `internal/chat/config` reads `MILES_PROVIDER_*` env vars, `ChatController` launches the sidecar with `QProcess`, contract checks `check_phase_2_0_ai_chat_mvp` and `check_phase_2_1_provider` pass.
- Qt 6.5+ is installed (Apple Silicon Homebrew under `/opt/homebrew`), as already required by Phase 2.0.
- macOS 12+ for `kSecAttrSynchronizable` and modern Security framework APIs; the build is currently Apple-Silicon only.
- The user is OK with the sidecar being restarted (≤1s downtime) when settings are saved.
- The reference background for this phase is `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md` §6 Phase 2.2.

## File Structure

Create:
- `apps/desktop/src/settings/SecretStore.h`
  - Pure-virtual `SecretStore` interface (`available()`, `read()`, `write()`, `remove()`). `inline static std::unique_ptr<SecretStore> create()` factory declaration. `NullSecretStore` class declaration (always-unavailable fallback).
- `apps/desktop/src/settings/SecretStore.cpp`
  - `SecretStore::create()` factory: on Apple, returns `MacSecretStore`; otherwise returns `NullSecretStore`. `NullSecretStore` method bodies.
- `apps/desktop/src/settings/MacSecretStore.h` (Apple-only)
  - `MacSecretStore` declaration deriving from `SecretStore`.
- `apps/desktop/src/settings/MacSecretStore.mm` (Apple-only)
  - Implementation using `SecItemAdd` / `SecItemCopyMatching` / `SecItemDelete` against the generic-password class.
- `apps/desktop/src/settings/SettingsService.h`
  - `SettingsService` `QObject` with non-secret getters/setters (`baseUrl`, `model`, `temperature`, `maxTokens`, `msPerChar`), API-key accessor + setter that goes through `SecretStore`, `save()` method, `saved()` signal, `secretStoreAvailable()` query.
- `apps/desktop/src/settings/SettingsService.cpp`
  - QSettings-backed persistence; secret routing.
- `apps/desktop/src/settings/SettingsController.h`
  - QML-singleton wrapper exposing properties + `Q_INVOKABLE save()`, with `notesText` / `secretStoreAvailable` for the UI to show the fallback warning. Includes `ChatControllerForeign`-style singleton boilerplate.
- `apps/desktop/src/settings/SettingsController.cpp`
  - Implementation forwarding to `SettingsService`; staging buffer so Cancel discards in-progress edits.
- `apps/desktop/qml/SettingsWindow.qml`
  - `ApplicationWindow` with form, masked API key field, fallback-warning Label, Save / Cancel buttons.
- `apps/desktop/tests/settings_service_smoke.cpp`
  - Round-trip tests for non-secret fields via a temporary `QSettings` scope, and API key tests using an `InMemorySecretStore` fixture defined inline in the test file.
- `tests/check_phase_2_2_settings.py`
  - Static contract check.
- `docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md`
  - Stage record produced in the final task.

Modify:
- `apps/desktop/src/chat/ChatController.h`
  - Add `SettingsService *m_settings = nullptr;`, new constructor parameter `SettingsService *settings`, new `Q_INVOKABLE void restartSidecar();` method, new slot `void handleSettingsSaved();`.
- `apps/desktop/src/chat/ChatController.cpp`
  - `startSidecar()` builds a `QProcessEnvironment` from `m_settings` and calls `m_sidecarProcess.setProcessEnvironment(env)` before `start()`. New `restartSidecar()` kills + restarts. New `handleSettingsSaved()` calls `restartSidecar()`.
- `apps/desktop/src/main.cpp`
  - Set `QCoreApplication::setOrganizationName("tian")` etc. before any `QSettings` access; construct `SettingsService` and `SettingsController`; pass `SettingsService*` into `ChatController`; connect `SettingsController::saved` → `ChatController::handleSettingsSaved`; load `SettingsWindow` QML (but keep it hidden until invoked).
- `apps/desktop/qml/ChatWindow.qml`
  - Add a "设置" button in the header that calls `SettingsController.openWindow()`.
- `apps/desktop/CMakeLists.txt`
  - Add the new sources to `DESKTOP_SOURCES`; add `MacSecretStore.{h,mm}` to the `APPLE` block; add `qml/SettingsWindow.qml` to `qt_add_qml_module`; link `"-framework Security"` on Apple; declare a new `SettingsServiceSmoke` test target.
- `CMakeLists.txt`
  - Register `check_phase_2_2_settings` ctest.
- `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`
  - In §6 Phase 2.2 block, add a "已完成" callout and a link to the stage record (final task only).
- `docs/v2/文档索引.md`
  - Add a link to the Phase 2.2 stage record.

Not modified (intentional):
- `apps/agent-core/**` — the sidecar continues to read `MILES_PROVIDER_*` env vars, no change to Go code.
- Existing PetRuntime / interaction / surface modules — unrelated.

---

## Task 0: Toolchain Preflight

**Files:**
- Read-only

- [ ] **Step 1: Verify branch state**

Run:

```bash
git status --short --branch
```

Expected: clean (or only intentional changes you're carrying forward). If the branch is `phase-2-1-provider` and Phase 2.1 has been merged to `main`, switch to `main` first via `git checkout main && git pull`. If a Phase 2.2 worktree is preferred, create one now using the `superpowers:using-git-worktrees` skill before continuing.

- [ ] **Step 2: Verify baseline tests pass**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && \
  ctest --test-dir build -R "check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider" --output-on-failure
```

Expected: both contract checks pass. (`check_phase_2_1_provider` was added by Phase 2.1.) If either fails, stop and fix before continuing — Phase 2.2 builds on top of these.

- [ ] **Step 3: Confirm Apple Security framework is available**

Run:

```bash
ls /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Security.framework 2>/dev/null || \
xcrun --show-sdk-path
```

Expected: either a `Security.framework` directory listing, or an SDK path is printed (the framework lives under `${SDK}/System/Library/Frameworks/Security.framework`). Both are fine — the framework is built into the macOS SDK; we just need the SDK present, which it must be since the existing build already compiles `MacPetWindowBehavior.mm`.

- [ ] **Step 4: No commit**

This task changes nothing.

---

## Task 1: Phase 2.2 Static Contract Scaffold

**Files:**
- Create: `tests/check_phase_2_2_settings.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the contract test**

Create `tests/check_phase_2_2_settings.py`:

```python
#!/usr/bin/env python3
"""Check the Phase 2.2 settings + secret-store contract."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    secret_h = read("apps/desktop/src/settings/SecretStore.h")
    secret_cpp = read("apps/desktop/src/settings/SecretStore.cpp")
    mac_secret_h = read("apps/desktop/src/settings/MacSecretStore.h")
    mac_secret_mm = read("apps/desktop/src/settings/MacSecretStore.mm")
    settings_h = read("apps/desktop/src/settings/SettingsService.h")
    settings_cpp = read("apps/desktop/src/settings/SettingsService.cpp")
    controller_h = read("apps/desktop/src/settings/SettingsController.h")
    controller_cpp = read("apps/desktop/src/settings/SettingsController.cpp")
    settings_qml = read("apps/desktop/qml/SettingsWindow.qml")
    chat_qml = read("apps/desktop/qml/ChatWindow.qml")
    chat_h = read("apps/desktop/src/chat/ChatController.h")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    main_cpp = read("apps/desktop/src/main.cpp")
    smoke = read("apps/desktop/tests/settings_service_smoke.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")
    index_doc = read("docs/v2/文档索引.md")
    stage_doc = read("docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md")

    # SecretStore interface
    require("class SecretStore" in secret_h, "SecretStore.h must declare class SecretStore")
    require("virtual bool available()" in secret_h, "SecretStore must expose available()")
    require("virtual QString read(" in secret_h, "SecretStore must expose read()")
    require("virtual bool write(" in secret_h, "SecretStore must expose write()")
    require("virtual bool remove(" in secret_h, "SecretStore must expose remove()")
    require("std::unique_ptr<SecretStore> create()" in secret_h, "factory create() must be declared")
    require("class NullSecretStore" in secret_h, "NullSecretStore must be declared")
    require("Q_OS_MACOS" in secret_cpp or "Q_OS_MAC" in secret_cpp,
            "SecretStore.cpp factory must branch on macOS")

    # macOS implementation
    require("Security/Security.h" in mac_secret_mm, "MacSecretStore.mm must include Security framework")
    require("SecItemAdd" in mac_secret_mm, "MacSecretStore must use SecItemAdd")
    require("SecItemCopyMatching" in mac_secret_mm, "MacSecretStore must use SecItemCopyMatching")
    require("SecItemDelete" in mac_secret_mm, "MacSecretStore must use SecItemDelete")
    require("kSecClassGenericPassword" in mac_secret_mm,
            "MacSecretStore must use generic-password keychain class")
    require("class MacSecretStore" in mac_secret_h,
            "MacSecretStore.h must declare class MacSecretStore")

    # SettingsService
    require("class SettingsService" in settings_h, "SettingsService class missing")
    require("baseUrl" in settings_h and "setBaseUrl" in settings_h, "baseUrl getter/setter required")
    require("model" in settings_h and "setModel" in settings_h, "model getter/setter required")
    require("temperature" in settings_h and "setTemperature" in settings_h,
            "temperature getter/setter required")
    require("maxTokens" in settings_h and "setMaxTokens" in settings_h,
            "maxTokens getter/setter required")
    require("msPerChar" in settings_h and "setMsPerChar" in settings_h,
            "msPerChar getter/setter required")
    require("apiKey" in settings_h and "setApiKey" in settings_h, "apiKey accessor/setter required")
    require("secretStoreAvailable" in settings_h, "secretStoreAvailable query required")
    require("void save()" in settings_h or "Q_INVOKABLE void save" in settings_h,
            "save() method required")
    require("void saved()" in settings_h, "saved() signal required")
    require("QSettings" in settings_cpp, "SettingsService must persist through QSettings")
    require("provider/baseUrl" in settings_cpp, "QSettings key provider/baseUrl required")
    require("provider/model" in settings_cpp, "QSettings key provider/model required")
    require("provider/temperature" in settings_cpp, "QSettings key provider/temperature required")
    require("provider/maxTokens" in settings_cpp, "QSettings key provider/maxTokens required")
    require("chat/msPerChar" in settings_cpp, "QSettings key chat/msPerChar required")
    require("apiKey" not in settings_cpp.replace("setApiKey", "").replace("apiKey()", "")
            or "QSettings" not in settings_cpp[settings_cpp.find("setApiKey"):],
            "API key must never be written via QSettings")
    # Spot-check: literal "provider/apiKey" must not appear (defensive)
    require("provider/apiKey" not in settings_cpp,
            "API key must never be stored as a QSettings key")

    # SettingsController + QML window
    require("class SettingsController" in controller_h, "SettingsController class missing")
    require("Q_INVOKABLE" in controller_h, "SettingsController must expose Q_INVOKABLE methods")
    require("openWindow" in controller_h, "SettingsController.openWindow() required")
    require("SettingsControllerForeign" in controller_h,
            "SettingsControllerForeign QML singleton boilerplate required")
    require("ApplicationWindow" in settings_qml, "SettingsWindow.qml must be ApplicationWindow")
    require("echoMode" in settings_qml, "API key field must use echoMode for masking")
    require("设置" in chat_qml, "ChatWindow must surface a 设置 button")
    require("SettingsController" in chat_qml,
            "ChatWindow must reference the SettingsController singleton")

    # ChatController wiring
    require("SettingsService" in chat_h, "ChatController must take a SettingsService")
    require("QProcessEnvironment" in chat_cpp, "ChatController must build a QProcessEnvironment")
    require("MILES_PROVIDER_BASE_URL" in chat_cpp,
            "ChatController must inject MILES_PROVIDER_BASE_URL")
    require("MILES_PROVIDER_API_KEY" in chat_cpp,
            "ChatController must inject MILES_PROVIDER_API_KEY")
    require("MILES_PROVIDER_MODEL" in chat_cpp,
            "ChatController must inject MILES_PROVIDER_MODEL")
    require("MILES_PROVIDER_TEMPERATURE" in chat_cpp,
            "ChatController must inject MILES_PROVIDER_TEMPERATURE")
    require("MILES_PROVIDER_MAX_TOKENS" in chat_cpp,
            "ChatController must inject MILES_PROVIDER_MAX_TOKENS")
    require("restartSidecar" in chat_h and "restartSidecar" in chat_cpp,
            "ChatController must expose restartSidecar()")
    require("handleSettingsSaved" in chat_h and "handleSettingsSaved" in chat_cpp,
            "ChatController must respond to saved settings")

    # main.cpp wiring
    require("setOrganizationName" in main_cpp, "main must set QCoreApplication organization name")
    require("setApplicationName" in main_cpp, "main must set QCoreApplication application name")
    require("SettingsService" in main_cpp, "main must construct SettingsService")
    require("SettingsController" in main_cpp, "main must construct SettingsController")
    require("loadFromModule" in main_cpp and "SettingsWindow" in main_cpp,
            "main must load SettingsWindow QML")

    # Tests
    require("InMemorySecretStore" in smoke, "smoke test must define InMemorySecretStore")
    require("msPerChar" in smoke, "smoke test must cover msPerChar persistence")
    require("apiKey" in smoke, "smoke test must cover apiKey routing through SecretStore")
    require("SettingsServiceSmoke" in desktop_cmake,
            "desktop CMake must register SettingsServiceSmoke target")
    require("settings_service_smoke" in desktop_cmake,
            "desktop CMake must register settings_service_smoke test")
    require("\"-framework Security\"" in desktop_cmake or "Security\"" in desktop_cmake,
            "desktop CMake must link Apple Security framework on macOS")
    require("MacSecretStore.mm" in desktop_cmake,
            "desktop CMake must list MacSecretStore.mm under the APPLE block")
    require("SettingsWindow.qml" in desktop_cmake,
            "desktop CMake must add SettingsWindow.qml to the QML module")
    require("check_phase_2_2_settings" in root_cmake,
            "root CMake must register Phase 2.2 contract check")

    # Docs
    require("Phase 2.2" in stage_doc, "Phase 2.2 stage record must exist")
    require("Phase 2.2" in index_doc, "doc index must link Phase 2.2 record")

    print("phase 2.2 settings contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Run to verify it fails**

Run:

```bash
python3 tests/check_phase_2_2_settings.py
```

Expected: `AssertionError: missing file: apps/desktop/src/settings/SecretStore.h`. The check fails fast on the first missing artifact, which is what we want.

- [ ] **Step 3: Register in root CMakeLists.txt**

Open `CMakeLists.txt`. Find the existing Phase 2.1 block:

```cmake
        # Phase 2.1 的 OpenAI-compatible provider 检查：守住
        # provider 配置、[EXPR:x] 解析、Qt 请求表达清单和阶段记录。
        add_test(
            NAME check_phase_2_1_provider
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_1_provider.py
        )
```

Append immediately after it (still inside the `if(BUILD_TESTING)` / `if(Python3_Interpreter_FOUND)` blocks):

```cmake
        # Phase 2.2 的用户配置与安全存储检查：守住设置面板、Keychain
        # 路由、msPerChar 持久化以及 ChatController QProcess env 注入。
        add_test(
            NAME check_phase_2_2_settings
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_2_settings.py
        )
```

- [ ] **Step 4: Verify CTest sees the new test**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew >/dev/null && \
  ctest --test-dir build -N -R check_phase_2_2_settings
```

Expected output contains `Test #N: check_phase_2_2_settings`.

- [ ] **Step 5: Commit**

```bash
git add tests/check_phase_2_2_settings.py CMakeLists.txt
git commit -m "test(phase-2-2): add settings + secret store contract scaffold (red)"
```

---

## Task 2: SecretStore Interface + NullSecretStore Fallback

**Files:**
- Create: `apps/desktop/src/settings/SecretStore.h`
- Create: `apps/desktop/src/settings/SecretStore.cpp`

- [ ] **Step 1: Write the header**

Create `apps/desktop/src/settings/SecretStore.h`:

```cpp
#pragma once

#include <QString>
#include <memory>

// SecretStore is a thin abstraction over the OS keychain (macOS Keychain,
// Windows Credential Manager, Linux Secret Service). The desktop reads the
// API key from here and injects it into the sidecar's QProcessEnvironment.
//
// When no real backend is available (current Windows / Linux build), the
// factory returns NullSecretStore, which reports `available() == false`.
// Callers must treat unavailability as "fall back to MILES_PROVIDER_API_KEY
// env var" — see SettingsService::effectiveApiKey().
class SecretStore
{
public:
    virtual ~SecretStore() = default;

    virtual bool available() const = 0;

    // Returns the secret stored under (service, account), or an empty string
    // if absent. On error, returns empty (errors are best-effort opaque here).
    virtual QString read(const QString &service, const QString &account) = 0;

    // Persists secret. Empty secret should be treated as "remove".
    // Returns false if the backend is unavailable or the write failed.
    virtual bool write(const QString &service, const QString &account,
                       const QString &secret) = 0;

    // Removes (service, account). Missing entries are not an error.
    virtual bool remove(const QString &service, const QString &account) = 0;

    // Factory: returns a real backend on macOS, a NullSecretStore elsewhere.
    static std::unique_ptr<SecretStore> create();
};

// NullSecretStore is the fallback returned on platforms without a real
// backend. Every method reports unavailable / empty.
class NullSecretStore : public SecretStore
{
public:
    bool available() const override { return false; }
    QString read(const QString &, const QString &) override { return {}; }
    bool write(const QString &, const QString &, const QString &) override { return false; }
    bool remove(const QString &, const QString &) override { return true; }
};
```

- [ ] **Step 2: Write the factory shim**

Create `apps/desktop/src/settings/SecretStore.cpp`:

```cpp
#include "SecretStore.h"

#ifdef Q_OS_MACOS
#include "MacSecretStore.h"
#endif

std::unique_ptr<SecretStore> SecretStore::create()
{
#ifdef Q_OS_MACOS
    return std::make_unique<MacSecretStore>();
#else
    return std::make_unique<NullSecretStore>();
#endif
}
```

- [ ] **Step 3: Verify the header is self-contained**

Run:

```bash
clang++ -std=c++17 -fsyntax-only -x c++ apps/desktop/src/settings/SecretStore.h \
  -I"$(brew --prefix qt)/include" \
  -I"$(brew --prefix qt)/include/QtCore" \
  -F"$(brew --prefix qt)/lib" \
  -DQT_NO_KEYWORDS 2>&1 | tail -5
```

Expected: no errors (it may print nothing). If clang can't find Qt headers, fall back to running the full CMake build in Step 5 — the header is small enough that any issue will surface there.

- [ ] **Step 4: No tests yet**

`MacSecretStore.h` does not exist yet, so a full build would fail. Skip the build until Task 3 lands the macOS implementation.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/settings/SecretStore.h apps/desktop/src/settings/SecretStore.cpp
git commit -m "feat(desktop): SecretStore interface + Null fallback"
```

---

## Task 3: macOS SecretStore Implementation

**Files:**
- Create: `apps/desktop/src/settings/MacSecretStore.h`
- Create: `apps/desktop/src/settings/MacSecretStore.mm`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `apps/desktop/src/settings/MacSecretStore.h`:

```cpp
#pragma once

#include "SecretStore.h"

class MacSecretStore : public SecretStore
{
public:
    bool available() const override { return true; }
    QString read(const QString &service, const QString &account) override;
    bool write(const QString &service, const QString &account,
               const QString &secret) override;
    bool remove(const QString &service, const QString &account) override;
};
```

- [ ] **Step 2: Write the Obj-C++ implementation**

Create `apps/desktop/src/settings/MacSecretStore.mm`:

```objc++
#include "MacSecretStore.h"

#import <Security/Security.h>
#import <Foundation/Foundation.h>

#include <QByteArray>

namespace {

NSData *toNSData(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return [NSData dataWithBytes:bytes.constData() length:static_cast<NSUInteger>(bytes.size())];
}

NSString *toNSString(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return [[NSString alloc] initWithBytes:bytes.constData()
                                    length:bytes.size()
                                  encoding:NSUTF8StringEncoding];
}

NSMutableDictionary *baseQuery(const QString &service, const QString &account)
{
    NSMutableDictionary *q = [NSMutableDictionary dictionary];
    q[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    q[(__bridge id)kSecAttrService] = toNSString(service);
    q[(__bridge id)kSecAttrAccount] = toNSString(account);
    return q;
}

} // namespace

QString MacSecretStore::read(const QString &service, const QString &account)
{
    NSMutableDictionary *q = baseQuery(service, account);
    q[(__bridge id)kSecReturnData] = (__bridge id)kCFBooleanTrue;
    q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)q, &result);
    if (status != errSecSuccess || result == nullptr) {
        return {};
    }

    NSData *data = (__bridge_transfer NSData *)result;
    return QString::fromUtf8(static_cast<const char *>(data.bytes),
                             static_cast<int>(data.length));
}

bool MacSecretStore::write(const QString &service, const QString &account,
                           const QString &secret)
{
    if (secret.isEmpty()) {
        return remove(service, account);
    }

    NSMutableDictionary *query = baseQuery(service, account);
    NSMutableDictionary *attrsToUpdate = [NSMutableDictionary dictionary];
    attrsToUpdate[(__bridge id)kSecValueData] = toNSData(secret);

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query,
                                    (__bridge CFDictionaryRef)attrsToUpdate);
    if (status == errSecSuccess) {
        return true;
    }
    if (status != errSecItemNotFound) {
        return false;
    }

    NSMutableDictionary *addQuery = baseQuery(service, account);
    addQuery[(__bridge id)kSecValueData] = toNSData(secret);
    status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    return status == errSecSuccess;
}

bool MacSecretStore::remove(const QString &service, const QString &account)
{
    NSMutableDictionary *q = baseQuery(service, account);
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)q);
    return status == errSecSuccess || status == errSecItemNotFound;
}
```

- [ ] **Step 3: Add to desktop CMakeLists**

Open `apps/desktop/CMakeLists.txt`. Find the `if(APPLE)` block (currently lines ~74-80):

```cmake
if(APPLE)
    enable_language(OBJCXX)
    list(APPEND DESKTOP_SOURCES
        src/platform/MacPetWindowBehavior.h
        src/platform/MacPetWindowBehavior.mm
    )
endif()
```

Replace it with:

```cmake
if(APPLE)
    enable_language(OBJCXX)
    list(APPEND DESKTOP_SOURCES
        src/platform/MacPetWindowBehavior.h
        src/platform/MacPetWindowBehavior.mm
        src/settings/MacSecretStore.h
        src/settings/MacSecretStore.mm
    )
endif()
```

Then find the `target_link_libraries(MilesEdgeworthDesktop ...)` block and add an Apple-only Security link. Append after the existing `target_link_libraries` call:

```cmake
if(APPLE)
    target_link_libraries(MilesEdgeworthDesktop PRIVATE "-framework Security")
endif()
```

- [ ] **Step 4: Verify build**

The `SettingsService` / `SettingsController` files don't exist yet, so the build is still not green end-to-end. But we can at least confirm the `MacSecretStore.mm` compiles in isolation by building the existing target — `SecretStore.cpp` is not yet in the source list, so build of the existing executable should still succeed.

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop 2>&1 | tail -20
```

Expected: existing target still builds. The two new files are not yet listed in `DESKTOP_SOURCES`, so the build does not yet exercise them. They get added in Task 4.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/settings/MacSecretStore.h \
        apps/desktop/src/settings/MacSecretStore.mm \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): MacSecretStore via Apple Security framework"
```

---

## Task 4: SettingsService — Non-Secret Fields (TDD)

**Files:**
- Create: `apps/desktop/tests/settings_service_smoke.cpp`
- Create: `apps/desktop/src/settings/SettingsService.h`
- Create: `apps/desktop/src/settings/SettingsService.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the failing smoke test (non-secret path)**

Create `apps/desktop/tests/settings_service_smoke.cpp`:

```cpp
#include "settings/SecretStore.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include <cassert>
#include <map>
#include <utility>

namespace {

// In-memory SecretStore used only by the smoke test — avoids touching the real
// macOS keychain in CI / dev runs.
class InMemorySecretStore : public SecretStore
{
public:
    bool available() const override { return true; }

    QString read(const QString &service, const QString &account) override
    {
        const auto it = m_store.find({service, account});
        return it == m_store.end() ? QString() : it->second;
    }

    bool write(const QString &service, const QString &account,
               const QString &secret) override
    {
        if (secret.isEmpty()) {
            m_store.erase({service, account});
        } else {
            m_store[{service, account}] = secret;
        }
        return true;
    }

    bool remove(const QString &service, const QString &account) override
    {
        m_store.erase({service, account});
        return true;
    }

private:
    std::map<std::pair<QString, QString>, QString> m_store;
};

void setupQSettingsScope(QTemporaryDir &dir)
{
    // Force QSettings to a per-test path so we don't clobber real user prefs.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName("tian-test");
    QCoreApplication::setApplicationName("MilesEdgeworth-test");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tmp;
    assert(tmp.isValid());
    setupQSettingsScope(tmp);

    {
        InMemorySecretStore store;
        SettingsService service(&store);

        // Defaults match the Go sidecar's defaults (apps/agent-core/internal/chat/config).
        assert(service.baseUrl().isEmpty());
        assert(service.model().isEmpty());
        assert(qFuzzyCompare(service.temperature() + 1.0, 0.7 + 1.0));
        assert(service.maxTokens() == 2048);
        assert(service.msPerChar() == 80);

        service.setBaseUrl(QStringLiteral("https://api.example.com"));
        service.setModel(QStringLiteral("gpt-test"));
        service.setTemperature(0.3);
        service.setMaxTokens(1024);
        service.setMsPerChar(120);
        service.save();
    }

    {
        InMemorySecretStore store;
        SettingsService service(&store);
        assert(service.baseUrl() == QStringLiteral("https://api.example.com"));
        assert(service.model() == QStringLiteral("gpt-test"));
        assert(qFuzzyCompare(service.temperature() + 1.0, 0.3 + 1.0));
        assert(service.maxTokens() == 1024);
        assert(service.msPerChar() == 120);
    }

    // API key round-trip via SecretStore — see Task 5.
    {
        InMemorySecretStore store;
        SettingsService service(&store);
        assert(service.apiKey().isEmpty());

        service.setApiKey(QStringLiteral("sk-secret"));
        service.save();
        assert(service.apiKey() == QStringLiteral("sk-secret"));

        // A fresh service reading from the same store should see it.
        SettingsService reopen(&store);
        assert(reopen.apiKey() == QStringLiteral("sk-secret"));

        // Clearing the key removes it from the backing store.
        reopen.setApiKey(QString());
        reopen.save();
        SettingsService reopen2(&store);
        assert(reopen2.apiKey().isEmpty());
    }

    return 0;
}
```

- [ ] **Step 2: Write the SettingsService header**

Create `apps/desktop/src/settings/SettingsService.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>

class SecretStore;

// SettingsService is the persistence layer for user-configurable values.
// Non-secret fields go through QSettings; the API key is routed through the
// injected SecretStore (macOS Keychain in production, InMemorySecretStore
// in tests). The API key is never written to QSettings, and is never sent
// over IPC — ChatController reads it here and only forwards it via the
// sidecar's QProcess environment.
class SettingsService : public QObject
{
    Q_OBJECT

public:
    // Service / account identifiers used against the SecretStore.
    static constexpr const char *kKeychainService = "dev.tian.MilesEdgeworth.v2";
    static constexpr const char *kKeychainAccount = "MILES_PROVIDER_API_KEY";

    explicit SettingsService(SecretStore *secretStore, QObject *parent = nullptr);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &value);

    QString model() const { return m_model; }
    void setModel(const QString &value);

    double temperature() const { return m_temperature; }
    void setTemperature(double value);

    int maxTokens() const { return m_maxTokens; }
    void setMaxTokens(int value);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    // Returns the API key currently held by the SecretStore (or the staged
    // value if the user edited it since the last save()).
    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &value);

    bool secretStoreAvailable() const;

    // Persists non-secret fields to QSettings and the API key to SecretStore.
    // Emits saved() on success. Fields that did not change are not rewritten.
    void save();

signals:
    void saved();

private:
    void load();

    SecretStore *m_secretStore = nullptr;
    QString m_baseUrl;
    QString m_model;
    double m_temperature = 0.7;
    int m_maxTokens = 2048;
    int m_msPerChar = 80;
    QString m_apiKey;
    QString m_savedApiKey;
};
```

- [ ] **Step 3: Write the SettingsService implementation**

Create `apps/desktop/src/settings/SettingsService.cpp`:

```cpp
#include "SettingsService.h"

#include "SecretStore.h"

#include <QSettings>

namespace {

constexpr const char *kBaseUrlKey = "provider/baseUrl";
constexpr const char *kModelKey = "provider/model";
constexpr const char *kTemperatureKey = "provider/temperature";
constexpr const char *kMaxTokensKey = "provider/maxTokens";
constexpr const char *kMsPerCharKey = "chat/msPerChar";

} // namespace

SettingsService::SettingsService(SecretStore *secretStore, QObject *parent)
    : QObject(parent), m_secretStore(secretStore)
{
    load();
}

void SettingsService::load()
{
    QSettings s;
    m_baseUrl = s.value(kBaseUrlKey).toString();
    m_model = s.value(kModelKey).toString();
    m_temperature = s.value(kTemperatureKey, 0.7).toDouble();
    m_maxTokens = s.value(kMaxTokensKey, 2048).toInt();
    m_msPerChar = s.value(kMsPerCharKey, 80).toInt();

    if (m_secretStore && m_secretStore->available()) {
        m_savedApiKey = m_secretStore->read(QString::fromUtf8(kKeychainService),
                                            QString::fromUtf8(kKeychainAccount));
        m_apiKey = m_savedApiKey;
    } else {
        m_savedApiKey.clear();
        m_apiKey.clear();
    }
}

void SettingsService::setBaseUrl(const QString &value) { m_baseUrl = value.trimmed(); }
void SettingsService::setModel(const QString &value) { m_model = value.trimmed(); }

void SettingsService::setTemperature(double value)
{
    if (value < 0.0) value = 0.0;
    if (value > 2.0) value = 2.0;
    m_temperature = value;
}

void SettingsService::setMaxTokens(int value)
{
    if (value < 1) value = 1;
    if (value > 32768) value = 32768;
    m_maxTokens = value;
}

void SettingsService::setMsPerChar(int value)
{
    if (value < 40) value = 40;
    if (value > 200) value = 200;
    m_msPerChar = value;
}

void SettingsService::setApiKey(const QString &value) { m_apiKey = value; }

bool SettingsService::secretStoreAvailable() const
{
    return m_secretStore && m_secretStore->available();
}

void SettingsService::save()
{
    {
        QSettings s;
        s.setValue(kBaseUrlKey, m_baseUrl);
        s.setValue(kModelKey, m_model);
        s.setValue(kTemperatureKey, m_temperature);
        s.setValue(kMaxTokensKey, m_maxTokens);
        s.setValue(kMsPerCharKey, m_msPerChar);
    }

    if (m_apiKey != m_savedApiKey && m_secretStore && m_secretStore->available()) {
        if (m_apiKey.isEmpty()) {
            m_secretStore->remove(QString::fromUtf8(kKeychainService),
                                  QString::fromUtf8(kKeychainAccount));
        } else {
            m_secretStore->write(QString::fromUtf8(kKeychainService),
                                 QString::fromUtf8(kKeychainAccount),
                                 m_apiKey);
        }
        m_savedApiKey = m_apiKey;
    }

    emit saved();
}
```

- [ ] **Step 4: Register the smoke test target**

Open `apps/desktop/CMakeLists.txt`. Find the existing `if(BUILD_TESTING)` block and the `add_test(NAME chat_controller_smoke ...)` line. Append a new target after it (still inside the `if(BUILD_TESTING)` block):

```cmake
    qt_add_executable(SettingsServiceSmoke
        tests/settings_service_smoke.cpp
        src/settings/SettingsService.cpp
        src/settings/SettingsService.h
        src/settings/SecretStore.cpp
        src/settings/SecretStore.h
    )

    target_include_directories(SettingsServiceSmoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(SettingsServiceSmoke
        PRIVATE
            Qt6::Core
    )

    add_test(NAME settings_service_smoke COMMAND SettingsServiceSmoke)
```

Note: do **not** add `MacSecretStore.{h,mm}` to this target. The smoke test uses `InMemorySecretStore`; we don't want to drag in the Security framework or hit the real Keychain. `SecretStore.cpp` references `MacSecretStore.h` only under `#ifdef Q_OS_MACOS`, but the test never calls `SecretStore::create()`. To keep this true even on macOS, guard `SecretStore.cpp`'s factory differently:

Replace `apps/desktop/src/settings/SecretStore.cpp` with:

```cpp
#include "SecretStore.h"

#ifdef MILES_HAS_MAC_SECRET_STORE
#include "MacSecretStore.h"
#endif

std::unique_ptr<SecretStore> SecretStore::create()
{
#ifdef MILES_HAS_MAC_SECRET_STORE
    return std::make_unique<MacSecretStore>();
#else
    return std::make_unique<NullSecretStore>();
#endif
}
```

Then in `apps/desktop/CMakeLists.txt`, inside the `if(APPLE)` block from Task 3, add the compile definition to the main target:

```cmake
if(APPLE)
    target_link_libraries(MilesEdgeworthDesktop PRIVATE "-framework Security")
    target_compile_definitions(MilesEdgeworthDesktop PRIVATE MILES_HAS_MAC_SECRET_STORE)
endif()
```

This way:
- `MilesEdgeworthDesktop` on macOS sees `MILES_HAS_MAC_SECRET_STORE`, includes the header, and links Security.framework.
- `SettingsServiceSmoke` does **not** see the macro, so its `SecretStore.cpp` resolves to `NullSecretStore` — and never references the `MacSecretStore` symbols.

- [ ] **Step 5: Build and run the smoke test**

Run:

```bash
cmake --build build --target SettingsServiceSmoke && \
  ctest --test-dir build -R settings_service_smoke --output-on-failure
```

Expected: target builds, test passes. If the test fails on the API-key portion (it shouldn't — the `InMemorySecretStore` is implemented in the test file), inspect output and revisit `SettingsService::save()` ordering.

- [ ] **Step 6: Verify check_phase_2_2_settings makes new progress**

Run:

```bash
python3 tests/check_phase_2_2_settings.py 2>&1 | head -5
```

Expected: still red, but failing on a later assertion (something about `SettingsController` or `SettingsWindow.qml`).

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/src/settings/SettingsService.h \
        apps/desktop/src/settings/SettingsService.cpp \
        apps/desktop/src/settings/SecretStore.cpp \
        apps/desktop/tests/settings_service_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): SettingsService persistence + smoke test"
```

---

## Task 5: SettingsController QML Singleton

**Files:**
- Create: `apps/desktop/src/settings/SettingsController.h`
- Create: `apps/desktop/src/settings/SettingsController.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write the header**

Create `apps/desktop/src/settings/SettingsController.h`:

```cpp
#pragma once

#include "SettingsService.h"

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

// SettingsController is the QML-facing wrapper around SettingsService.
// It keeps a staged copy of every editable field so Cancel can discard
// in-progress edits without touching the service.
class SettingsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY maxTokensChanged)
    Q_PROPERTY(int msPerChar READ msPerChar WRITE setMsPerChar NOTIFY msPerCharChanged)
    Q_PROPERTY(bool secretStoreAvailable READ secretStoreAvailable CONSTANT)
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    explicit SettingsController(SettingsService *service, QObject *parent = nullptr);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &value);

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &value);

    QString model() const { return m_model; }
    void setModel(const QString &value);

    double temperature() const { return m_temperature; }
    void setTemperature(double value);

    int maxTokens() const { return m_maxTokens; }
    void setMaxTokens(int value);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    bool secretStoreAvailable() const;
    bool windowVisible() const { return m_windowVisible; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void closeWindow();
    // Commits the staged values to SettingsService and emits saved().
    Q_INVOKABLE void save();
    // Drops staged edits and reloads from SettingsService.
    Q_INVOKABLE void revert();

signals:
    void baseUrlChanged();
    void apiKeyChanged();
    void modelChanged();
    void temperatureChanged();
    void maxTokensChanged();
    void msPerCharChanged();
    void windowVisibleChanged();
    void saved();

private:
    void syncFromService();

    SettingsService *m_service = nullptr;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    double m_temperature = 0.7;
    int m_maxTokens = 2048;
    int m_msPerChar = 80;
    bool m_windowVisible = false;
};

struct SettingsControllerForeign
{
    Q_GADGET
    QML_FOREIGN(SettingsController)
    QML_NAMED_ELEMENT(SettingsController)
    QML_SINGLETON

public:
    inline static SettingsController *s_instance = nullptr;

    static SettingsController *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
```

- [ ] **Step 2: Write the implementation**

Create `apps/desktop/src/settings/SettingsController.cpp`:

```cpp
#include "SettingsController.h"

SettingsController::SettingsController(SettingsService *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    syncFromService();
}

void SettingsController::syncFromService()
{
    if (!m_service) return;
    m_baseUrl = m_service->baseUrl();
    m_apiKey = m_service->apiKey();
    m_model = m_service->model();
    m_temperature = m_service->temperature();
    m_maxTokens = m_service->maxTokens();
    m_msPerChar = m_service->msPerChar();
    emit baseUrlChanged();
    emit apiKeyChanged();
    emit modelChanged();
    emit temperatureChanged();
    emit maxTokensChanged();
    emit msPerCharChanged();
}

void SettingsController::setBaseUrl(const QString &value)
{
    if (m_baseUrl == value) return;
    m_baseUrl = value;
    emit baseUrlChanged();
}

void SettingsController::setApiKey(const QString &value)
{
    if (m_apiKey == value) return;
    m_apiKey = value;
    emit apiKeyChanged();
}

void SettingsController::setModel(const QString &value)
{
    if (m_model == value) return;
    m_model = value;
    emit modelChanged();
}

void SettingsController::setTemperature(double value)
{
    if (qFuzzyCompare(m_temperature + 1.0, value + 1.0)) return;
    m_temperature = value;
    emit temperatureChanged();
}

void SettingsController::setMaxTokens(int value)
{
    if (m_maxTokens == value) return;
    m_maxTokens = value;
    emit maxTokensChanged();
}

void SettingsController::setMsPerChar(int value)
{
    if (m_msPerChar == value) return;
    m_msPerChar = value;
    emit msPerCharChanged();
}

bool SettingsController::secretStoreAvailable() const
{
    return m_service && m_service->secretStoreAvailable();
}

void SettingsController::openWindow()
{
    syncFromService();
    if (m_windowVisible) return;
    m_windowVisible = true;
    emit windowVisibleChanged();
}

void SettingsController::closeWindow()
{
    if (!m_windowVisible) return;
    m_windowVisible = false;
    emit windowVisibleChanged();
}

void SettingsController::save()
{
    if (!m_service) return;
    m_service->setBaseUrl(m_baseUrl);
    m_service->setApiKey(m_apiKey);
    m_service->setModel(m_model);
    m_service->setTemperature(m_temperature);
    m_service->setMaxTokens(m_maxTokens);
    m_service->setMsPerChar(m_msPerChar);
    m_service->save();
    emit saved();
    closeWindow();
}

void SettingsController::revert()
{
    syncFromService();
    closeWindow();
}
```

- [ ] **Step 3: Add to desktop CMakeLists**

Open `apps/desktop/CMakeLists.txt`. Find `DESKTOP_SOURCES` and append (alphabetical-by-path):

```cmake
    src/settings/SecretStore.cpp
    src/settings/SecretStore.h
    src/settings/SettingsController.cpp
    src/settings/SettingsController.h
    src/settings/SettingsService.cpp
    src/settings/SettingsService.h
```

Then find the `qt_add_qml_module(...)` block and add `SettingsController.h` to its `SOURCES` list (so the QML singleton boilerplate gets visible to MOC for the module):

```cmake
qt_add_qml_module(MilesEdgeworthDesktop
    URI MilesEdgeworth
    VERSION 1.0
    QML_FILES
        qml/ChatWindow.qml
        qml/PetWindow.qml
        qml/SettingsWindow.qml
    SOURCES
        src/DesktopShellController.h
        src/chat/ChatController.h
        src/pet/events/PetEventBridge.h
        src/pet/PetRuntime.h
        src/settings/SettingsController.h
)
```

(`SettingsWindow.qml` will be created in Task 6; CMake will fail to configure if the file is missing, so we add it in the same step that creates the file. For now, create an empty placeholder so configure passes:)

Run:

```bash
touch apps/desktop/qml/SettingsWindow.qml
```

- [ ] **Step 4: Reconfigure and build**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew && \
  cmake --build build --target MilesEdgeworthDesktop 2>&1 | tail -20
```

Expected: build succeeds. SettingsController is now wired into the executable. (QML can't import it yet without `SettingsWindow.qml` and `main.cpp` instantiating it — Task 6 and Task 8.)

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/settings/SettingsController.h \
        apps/desktop/src/settings/SettingsController.cpp \
        apps/desktop/qml/SettingsWindow.qml \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): SettingsController QML singleton scaffold"
```

---

## Task 6: SettingsWindow QML

**Files:**
- Modify: `apps/desktop/qml/SettingsWindow.qml`

- [ ] **Step 1: Replace placeholder with the real form**

Overwrite `apps/desktop/qml/SettingsWindow.qml`:

```qml
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import MilesEdgeworth

ApplicationWindow {
    id: settingsWindow
    width: 520
    height: 560
    minimumWidth: 480
    minimumHeight: 520
    visible: SettingsController.windowVisible
    title: qsTr("Miles 设置")
    flags: Qt.Dialog

    onVisibleChanged: {
        if (!visible) {
            // 用户点系统关闭按钮 → 等价于 Cancel。
            SettingsController.revert()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Provider 配置")
            font.pixelSize: 18
            font.bold: true
        }

        Label {
            visible: !SettingsController.secretStoreAvailable
            text: qsTr("⚠️ 系统 Keychain 不可用,API Key 仅从环境变量读取,不会写入磁盘。")
            color: "#a35200"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 8
            Layout.fillWidth: true

            Label { text: qsTr("Base URL") }
            TextField {
                id: baseUrlField
                Layout.fillWidth: true
                text: SettingsController.baseUrl
                placeholderText: "https://api.openai.com"
                onEditingFinished: SettingsController.baseUrl = text
            }

            Label { text: qsTr("API Key") }
            TextField {
                id: apiKeyField
                Layout.fillWidth: true
                text: SettingsController.apiKey
                echoMode: TextInput.Password
                placeholderText: "sk-..."
                onEditingFinished: SettingsController.apiKey = text
            }

            Label { text: qsTr("Model") }
            TextField {
                id: modelField
                Layout.fillWidth: true
                text: SettingsController.model
                placeholderText: "gpt-4o-mini"
                onEditingFinished: SettingsController.model = text
            }

            Label { text: qsTr("Temperature") }
            RowLayout {
                Layout.fillWidth: true
                Slider {
                    id: temperatureSlider
                    Layout.fillWidth: true
                    from: 0.0
                    to: 2.0
                    stepSize: 0.05
                    value: SettingsController.temperature
                    onValueChanged: SettingsController.temperature = value
                }
                Label {
                    text: temperatureSlider.value.toFixed(2)
                    Layout.preferredWidth: 40
                }
            }

            Label { text: qsTr("Max Tokens") }
            SpinBox {
                id: maxTokensField
                Layout.fillWidth: true
                from: 1
                to: 32768
                stepSize: 64
                value: SettingsController.maxTokens
                editable: true
                onValueChanged: SettingsController.maxTokens = value
            }

            Label { text: qsTr("文字节奏 (ms/字)") }
            RowLayout {
                Layout.fillWidth: true
                Slider {
                    id: msPerCharSlider
                    Layout.fillWidth: true
                    from: 40
                    to: 200
                    stepSize: 5
                    value: SettingsController.msPerChar
                    onValueChanged: SettingsController.msPerChar = value
                }
                Label {
                    text: Math.round(msPerCharSlider.value) + " ms"
                    Layout.preferredWidth: 60
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("取消")
                onClicked: SettingsController.revert()
            }
            Button {
                text: qsTr("保存")
                highlighted: true
                onClicked: {
                    // 任何 TextField 可能还没触发 editingFinished,显式 push 一次。
                    SettingsController.baseUrl = baseUrlField.text
                    SettingsController.apiKey = apiKeyField.text
                    SettingsController.model = modelField.text
                    SettingsController.save()
                }
            }
        }
    }
}
```

- [ ] **Step 2: Verify QML compiles**

The QML module is compiled by `qmltyperegistrar` at build time. Rebuild:

```bash
cmake --build build --target MilesEdgeworthDesktop 2>&1 | tail -30
```

Expected: success. If you see a complaint about `SettingsController` not being a QML element, double-check that `SettingsControllerForeign` was added to `qt_add_qml_module` SOURCES and that the singleton was registered (Task 5 Step 3).

- [ ] **Step 3: Commit**

```bash
git add apps/desktop/qml/SettingsWindow.qml
git commit -m "feat(desktop): SettingsWindow QML form"
```

---

## Task 7: ChatController — Read Settings + Inject into QProcess (TDD)

**Files:**
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Read current ChatController.cpp before editing**

Run:

```bash
sed -n '1,80p' apps/desktop/src/chat/ChatController.cpp
```

Locate the constructor and the `startSidecar()` body. You will modify:
- The constructor to take a `SettingsService *settings` parameter and store it.
- `startSidecar()` to build a `QProcessEnvironment` from `m_settings` and call `m_sidecarProcess.setProcessEnvironment(env)` before `start()`.

- [ ] **Step 2: Update the header**

Open `apps/desktop/src/chat/ChatController.h`. Add a forward declaration above the class:

```cpp
class PetRuntime;
class SettingsService;
```

Change the constructor declaration to:

```cpp
    explicit ChatController(PetRuntime *runtime, SettingsService *settings,
                            QObject *parent = nullptr);
```

Inside the class, add public:

```cpp
    Q_INVOKABLE void restartSidecar();
```

Add public slots:

```cpp
public slots:
    void handleSettingsSaved();
```

(Or use a private slot — the saved signal connection in `main.cpp` will work with any access level when using new-style `connect`.)

Add a private member:

```cpp
    SettingsService *m_settings = nullptr;
```

- [ ] **Step 3: Update the implementation**

Open `apps/desktop/src/chat/ChatController.cpp`. Update the include block to add:

```cpp
#include "settings/SettingsService.h"

#include <QProcessEnvironment>
```

Update the constructor signature. Find the existing constructor (it starts with `ChatController::ChatController(PetRuntime *runtime, ...)`) and change it to:

```cpp
ChatController::ChatController(PetRuntime *runtime, SettingsService *settings, QObject *parent)
    : QObject(parent),
      m_runtime(runtime),
      m_settings(settings)
{
    // ... existing body unchanged ...
}
```

Update `startSidecar()`. Find the existing body — it looks roughly like:

```cpp
void ChatController::startSidecar()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        return;
    }

    const QString executable = sidecarExecutablePath();
    if (executable.isEmpty()) {
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    m_sidecarProcess.start(executable, QStringList());
}
```

Replace the body with:

```cpp
void ChatController::startSidecar()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        return;
    }

    const QString executable = sidecarExecutablePath();
    if (executable.isEmpty()) {
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_settings) {
        // Only override when a value is set; otherwise let an externally-set
        // MILES_PROVIDER_* env survive (the "C: env-var only" fallback when
        // SecretStore is unavailable).
        if (!m_settings->baseUrl().isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_BASE_URL"), m_settings->baseUrl());
        }
        const QString key = m_settings->apiKey();
        if (!key.isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_API_KEY"), key);
        }
        if (!m_settings->model().isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_MODEL"), m_settings->model());
        }
        env.insert(QStringLiteral("MILES_PROVIDER_TEMPERATURE"),
                   QString::number(m_settings->temperature()));
        env.insert(QStringLiteral("MILES_PROVIDER_MAX_TOKENS"),
                   QString::number(m_settings->maxTokens()));
    }
    m_sidecarProcess.setProcessEnvironment(env);

    m_sidecarProcess.start(executable, QStringList());
}
```

Add `restartSidecar()` near `startSidecar()`:

```cpp
void ChatController::restartSidecar()
{
    setStatusText(QStringLiteral("正在重启 sidecar..."));
    setSidecarReady(false);

    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        m_sidecarProcess.terminate();
        if (!m_sidecarProcess.waitForFinished(2000)) {
            m_sidecarProcess.kill();
            m_sidecarProcess.waitForFinished(2000);
        }
    }
    startSidecar();
}

void ChatController::handleSettingsSaved()
{
    restartSidecar();
}
```

- [ ] **Step 4: Update the ChatController smoke test**

Open `apps/desktop/tests/chat_controller_smoke.cpp`. At the top, add the `#include` for `SettingsService` / `SecretStore`. Find the existing instantiation of `ChatController` — it currently looks like:

```cpp
ChatController controller(&runtime);
```

Replace each occurrence with the new shape that also constructs a SettingsService backed by an `InMemorySecretStore`. Add this near the top of the test file (above `main()`):

```cpp
#include "settings/SecretStore.h"
#include "settings/SettingsService.h"

namespace {
class TestSecretStore : public SecretStore
{
public:
    bool available() const override { return true; }
    QString read(const QString &, const QString &) override { return m_value; }
    bool write(const QString &, const QString &, const QString &value) override
    {
        m_value = value;
        return true;
    }
    bool remove(const QString &, const QString &) override
    {
        m_value.clear();
        return true;
    }
private:
    QString m_value;
};
} // namespace
```

In `main()`, before constructing `ChatController`, add:

```cpp
    QCoreApplication::setOrganizationName("tian-test");
    QCoreApplication::setApplicationName("MilesEdgeworth-smoke");

    TestSecretStore secretStore;
    SettingsService settingsService(&secretStore);
    settingsService.setBaseUrl(QStringLiteral("https://example.test"));
    settingsService.setModel(QStringLiteral("smoke-model"));
    settingsService.setApiKey(QStringLiteral("sk-smoke"));
    settingsService.save();
```

Then change every `ChatController controller(&runtime);` to:

```cpp
    ChatController controller(&runtime, &settingsService);
```

Append a new test block in `main()` after the existing assertions (and before returning 0) that exercises hold-buffer + the new env-injection path:

```cpp
    // Phase 2.2: startSidecar() must use the SettingsService values to populate
    // the QProcess environment before launching. We can't easily inspect the
    // child process here, so this test asserts the higher-level invariant:
    // calling startSidecar() should not crash even when the sidecar binary is
    // absent (the binary lookup returns empty, status text reflects that, and
    // we don't blow up while computing the QProcessEnvironment).
    controller.startSidecar();

    // Hold buffer round-trip (carried over from Phase 2.1 — kept here so future
    // refactors keep the smoke test honest).
    // ... existing hold buffer assertions ...
```

If the existing test file already has hold-buffer assertions, keep them as-is. If not, the placeholder comment above documents that this is the test's responsibility.

- [ ] **Step 5: Update the ChatControllerSmoke CMake target**

In `apps/desktop/CMakeLists.txt`, find the `qt_add_executable(ChatControllerSmoke ...)` block. Add the settings sources:

```cmake
        src/settings/SettingsService.cpp
        src/settings/SettingsService.h
        src/settings/SecretStore.cpp
        src/settings/SecretStore.h
```

(Add them to the sources list — alphabetical-by-path preferred, but match existing style.)

- [ ] **Step 6: Build and run the updated smoke test**

Run:

```bash
cmake --build build --target ChatControllerSmoke && \
  ctest --test-dir build -R chat_controller_smoke --output-on-failure
```

Expected: builds, passes. If a crash occurs in `startSidecar()` because `sidecarExecutablePath()` returns empty in the test runner context (no installed `miles-agent` next to the test binary), the function short-circuits before touching `QProcess`, so this should remain safe.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h \
        apps/desktop/src/chat/ChatController.cpp \
        apps/desktop/tests/chat_controller_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "feat(desktop): inject provider settings into sidecar QProcess env"
```

---

## Task 8: Wire It All Up in main.cpp

**Files:**
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/qml/ChatWindow.qml`

- [ ] **Step 1: Update main.cpp**

Open `apps/desktop/src/main.cpp`. Add includes at the top:

```cpp
#include "settings/SecretStore.h"
#include "settings/SettingsController.h"
#include "settings/SettingsService.h"
```

Inside `main()`, immediately after `QApplication app(argc, argv);`, set org/app names so `QSettings` lands at the expected macOS plist:

```cpp
    QCoreApplication::setOrganizationName(QStringLiteral("tian"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("dev.tian.MilesEdgeworth"));
    QCoreApplication::setApplicationName(QStringLiteral("MilesEdgeworth"));
```

After `PetRuntime petRuntime;` and before `ChatController chatController(&petRuntime);`, construct the settings stack:

```cpp
    std::unique_ptr<SecretStore> secretStore = SecretStore::create();
    SettingsService settingsService(secretStore.get());
    SettingsController settingsController(&settingsService);
    SettingsControllerForeign::s_instance = &settingsController;
```

Replace the existing `ChatController chatController(&petRuntime);` with:

```cpp
    ChatController chatController(&petRuntime, &settingsService);
    ChatControllerForeign::s_instance = &chatController;
    QObject::connect(&settingsController, &SettingsController::saved,
                     &chatController, &ChatController::handleSettingsSaved);
```

(`ChatControllerForeign::s_instance = ...` line should already exist from Phase 2.0/2.1; keep it as-is and just insert the new connect.)

After `chatEngine.loadFromModule("MilesEdgeworth", "ChatWindow");` and the existing guard, load the SettingsWindow into the **same** QML engine (a SettingsController-owned `windowVisible` property drives visibility, so the window can stay loaded but invisible):

```cpp
    chatEngine.loadFromModule("MilesEdgeworth", "SettingsWindow");
```

Don't add a separate engine for the settings window — sharing the engine is enough because the QML singleton owns the visibility state.

Make sure `<memory>` is included so `std::unique_ptr` resolves; on macOS Qt usually pulls it in transitively, but add an explicit `#include <memory>` near the top to be safe.

- [ ] **Step 2: Add the 设置 entry to ChatWindow.qml**

Open `apps/desktop/qml/ChatWindow.qml`. Locate the header / title bar Row (the area that already contains the status text or new-conversation button — find an existing `RowLayout` near the top of the page). Add a button:

```qml
        Button {
            text: qsTr("设置")
            onClicked: SettingsController.openWindow()
        }
```

Add the import at the top of `ChatWindow.qml` if it isn't already present:

```qml
import MilesEdgeworth
```

(This module is already imported by the existing `ChatController` reference — verify with a quick scan; if so, no new import is needed.)

- [ ] **Step 3: Build and run the desktop app manually**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop && \
  open build/apps/desktop/MilesEdgeworth\ v2.app 2>/dev/null || \
  ./build/apps/desktop/MilesEdgeworth\ v2.app/Contents/MacOS/MilesEdgeworthDesktop
```

Manual checks (each should pass):

1. ChatWindow opens with a 设置 button in the header.
2. Clicking 设置 opens the SettingsWindow.
3. Fill in: Base URL = `https://api.openai.com`, API Key = a real or fake `sk-...`, Model = `gpt-4o-mini`. Click 保存.
4. The settings window closes.
5. Status text briefly shows "正在重启 sidecar…", then returns to normal.
6. Reopen the settings window — values are still there (apart from API key visibility, which is masked but the field still shows dots reflecting the stored value).
7. Quit + relaunch the app. Reopen settings — fields persist; API key (dots) is also restored.
8. In macOS Keychain Access app, search for `dev.tian.MilesEdgeworth.v2` — an entry with account `MILES_PROVIDER_API_KEY` should exist.

If any of these fail, stop and inspect logs. Common failure modes:

- "Module MilesEdgeworth not installed" → check `qt_add_qml_module` SOURCES list includes `SettingsController.h`.
- API key not written to Keychain → confirm `MILES_HAS_MAC_SECRET_STORE` was defined on the desktop target.
- Sidecar didn't restart on save → check the `connect(&settingsController, &SettingsController::saved, ...)` line is present.

- [ ] **Step 4: Commit**

```bash
git add apps/desktop/src/main.cpp apps/desktop/qml/ChatWindow.qml
git commit -m "feat(desktop): wire settings UI + sidecar reload into main shell"
```

---

## Task 9: Final Verification + Contract Check Green

**Files:**
- No new files; verifies everything in concert.

- [ ] **Step 1: Run the contract check**

Run:

```bash
python3 tests/check_phase_2_2_settings.py
```

Expected: `phase 2.2 settings contract ok`. If any assertion still fails, fix the corresponding file inline (do not silently relax the assertion).

- [ ] **Step 2: Run all Go tests**

Run:

```bash
(cd apps/agent-core && go test ./...)
```

Expected: all green. The sidecar should be untouched — we only changed how the env vars are populated upstream.

- [ ] **Step 3: Run all CTest checks**

Run:

```bash
cmake --build build --target MilesEdgeworthDesktop && \
  ctest --test-dir build --output-on-failure \
    -R "settings_service_smoke|chat_controller_smoke|chat_stream_event_parser_smoke|check_phase_2_0_ai_chat_mvp|check_phase_2_1_provider|check_phase_2_2_settings|pet_runtime_smoke|skin_manifest_loader_smoke"
```

Expected: every listed test passes.

- [ ] **Step 4: End-to-end manual test against a real provider (optional but recommended)**

If you have a real OpenAI-compatible endpoint and key handy:

1. Open settings, fill real `https://api.openai.com` + key + `gpt-4o-mini` + Save.
2. Send "你好" in chat.
3. Confirm the reply streams with `[EXPR:tag]` markers driving pet animation.
4. Open settings again, change the model to a wrong name, save.
5. Send another message. Expect a `RUN_ERROR` event surfaced as a status line.
6. Restore the working settings, save, confirm chat works again — proves the live-reload path is solid.

- [ ] **Step 5: No commit (verification only)**

This task only verifies the state already committed in earlier tasks.

---

## Task 10: Stage Record + Docs

**Files:**
- Create: `docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md`
- Modify: `docs/v2/文档索引.md`
- Modify: `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`

- [ ] **Step 1: Write the stage record**

Create `docs/v2/阶段记录/Phase 2.2 用户配置与安全存储.md`:

```markdown
# Phase 2.2 用户配置与安全存储

本文记录 Phase 2.2 的实现范围、验收方式和当前限制。Phase 2.2 把 provider 配置从环境变量升级为图形化设置面板,API key 通过 macOS Keychain 存储,sidecar 通过 QProcess 环境变量接收配置,不再要求用户手动 `export` 环境变量。

参考粗规划:`docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md` §6 Phase 2.2。

## 完成范围

- 新增 `apps/desktop/src/settings/SecretStore.{h,cpp}`:跨平台 SecretStore 抽象 + `NullSecretStore` 兜底。`SecretStore::create()` 在 macOS 返回 `MacSecretStore`,其他平台返回 `NullSecretStore`。
- 新增 `apps/desktop/src/settings/MacSecretStore.{h,mm}`(Apple-only):基于 Security framework 的 generic-password Keychain 实现。
- 新增 `apps/desktop/src/settings/SettingsService.{h,cpp}`:QSettings 持久化 + SecretStore 路由,默认值与 Go sidecar 对齐(temperature 0.7、maxTokens 2048、msPerChar 80)。API key 永不写入 QSettings。
- 新增 `apps/desktop/src/settings/SettingsController.{h,cpp}`:QML 单例外观,带 staged 缓冲,Cancel 丢弃未保存编辑。
- 新增 `apps/desktop/qml/SettingsWindow.qml`:provider / base URL / API key (masked) / model / temperature / max tokens / msPerChar 表单,Keychain 不可用时显示橙色 banner。
- `apps/desktop/qml/ChatWindow.qml` 顶部新增 "设置" 按钮触发 `SettingsController.openWindow()`。
- `apps/desktop/src/chat/ChatController` 接受 `SettingsService *`,`startSidecar()` 从设置构建 `QProcessEnvironment` 注入 `MILES_PROVIDER_*`;响应 `SettingsController::saved` 重启 sidecar。
- `apps/desktop/src/main.cpp` 设置 `QCoreApplication` org/app name,构造 SettingsService / SettingsController,装载 SettingsWindow QML,把 `saved` 信号连到 ChatController。
- `apps/desktop/CMakeLists.txt` 链接 `-framework Security`,macOS 目标定义 `MILES_HAS_MAC_SECRET_STORE`,新增 `SettingsServiceSmoke` ctest 目标。
- 新增 `apps/desktop/tests/settings_service_smoke.cpp` 覆盖非密字段持久化 + API key 经 `InMemorySecretStore` 的路由。
- 新增 `tests/check_phase_2_2_settings.py` 静态契约。

## 验收命令

```bash
python3 tests/check_phase_2_0_ai_chat_mvp.py
python3 tests/check_phase_2_1_provider.py
python3 tests/check_phase_2_2_settings.py
(cd apps/agent-core && go test ./...)
cmake --build build --target MilesEdgeworthDesktop
ctest --test-dir build -R "settings_service_smoke|chat_controller_smoke|check_phase_2_2_settings" --output-on-failure
```

手动验收:打开 ChatWindow → 设置 → 填入 base URL / API key / model → 保存 → 状态栏出现"正在重启 sidecar"后恢复 → 在 macOS Keychain Access 中确认 `dev.tian.MilesEdgeworth.v2 / MILES_PROVIDER_API_KEY` 条目存在 → 发送消息验证 provider 工作。

## 关键决策

- **Keychain 不可用时的兜底**:env-var only(粗规划 TODO C 方案)。`ChatController::startSidecar()` 仅在 `SettingsService` 字段非空时插入 env,允许外部已设置的 `MILES_PROVIDER_*` 透传。API key 任何情况下都不会落盘。
- **配置流向**:Qt 端读 Keychain → 注入 `QProcessEnvironment` → sidecar 启动时通过现有 `internal/chat/config.FromEnv()` 消费。sidecar 代码 Phase 2.1 → 2.2 零改动。
- **Settings UI**:独立 `SettingsWindow.qml`,与 ChatWindow 解耦,便于 Phase 3+ 加更多设置项。
- **msPerChar**:字段已暴露并持久化(40–200ms 滑杆,默认 80),Phase 2.3 的字符速率限制器将读取该值,本阶段不消费。
- **Sidecar 热重载**:保存后 `restartSidecar()` `terminate → waitForFinished(2000) → kill → restart`。不引入 `/v1/config` HTTP 路由。

## 当前限制

- 只有 macOS Keychain 实装;Windows / Linux 落到 `NullSecretStore`,实际使用必须依靠 `MILES_PROVIDER_API_KEY` 环境变量。
- 没有"测试连接"按钮;用户保存后通过发送一条消息验证。
- 没有多 profile / 多 provider 切换 UI。
- `msPerChar` 设置目前只持久化,Phase 2.3 的字符速率限制器才会读取并产生可见效果。
- 没有 Langfuse 接入(粗规划已记录,Phase 2.3 一并完成)。

## 后续入口

- Phase 2.3:会话历史持久化、完整 Miles persona prompt、ChatController 完整状态机、字符速率限制器(消费 `msPerChar`)、Langfuse 接入。
- Phase 2.4:Phased 动画与 `requestCleanFinishAndNotify` / `requestBoundaryAndNotify`。
```

- [ ] **Step 2: Update the doc index**

Open `docs/v2/文档索引.md`. Find the existing Phase 2.1 entry under 阶段记录:

```markdown
- [Phase 2.1 OpenAI-compatible Provider](阶段记录/Phase%202.1%20OpenAI-compatible%20Provider.md)
```

Append after it:

```markdown
- [Phase 2.2 用户配置与安全存储](阶段记录/Phase%202.2%20用户配置与安全存储.md):Phase 2.2 的 SecretStore 抽象、macOS Keychain 接入、SettingsService/Controller、SettingsWindow QML 和 QProcess env 注入的验收记录。
```

- [ ] **Step 3: Update the rough plan with a "已完成" callout**

Open `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`. Find the Phase 2.2 section header:

```markdown
### Phase 2.2：用户配置与安全存储
```

Insert immediately below it:

```markdown
> 已完成。详见 `阶段记录/Phase 2.2 用户配置与安全存储.md`。
```

Also remove or strike-through the TODO callout at the bottom of the Phase 2.2 block (the keychain fallback TODO), since we've now picked C (env-var only). Replace:

```markdown
  - **TODO（Phase 2.2 详细计划时拍板）**：当 keychain / credential manager / secret service 不可用时的兜底——三选一：
    - A：拒绝启动并提示用户手动配置
    - B：fallback 到加密配置文件 + 显式警告
    - C：fallback 到仅环境变量读取（不在磁盘留任何 key）
  - 粗规划阶段不预先选定；Phase 2.2 plan 写具体方案时一并决定。
```

With:

```markdown
  - 兜底:Keychain 不可用时仅读 `MILES_PROVIDER_API_KEY` 环境变量,任何情况下 API key 不落盘。
```

- [ ] **Step 4: Update the "后续入口" pointer**

Open `docs/v2/阶段记录/Phase 2 AI 聊天粗规划.md`. Find the "## 8. 后续入口" section. Append:

```markdown
Phase 2.2 已完成,Phase 2.3 详细执行计划开始前请先读《AI 聊天动画编排设计》§5、§7。
```

- [ ] **Step 5: Run contract check + commit**

Run:

```bash
python3 tests/check_phase_2_2_settings.py
```

Expected: green.

```bash
git add docs/v2/阶段记录/Phase\ 2.2\ 用户配置与安全存储.md \
        docs/v2/文档索引.md \
        docs/v2/阶段记录/Phase\ 2\ AI\ 聊天粗规划.md
git commit -m "docs(phase-2-2): stage record + index + close TODO"
```

---

## Self-Review

### 1. Spec coverage

Walking through Phase 2.2 §6 of the rough plan:

- **基础设置窗口或设置面板** → Task 6 (SettingsWindow.qml) + Task 5 (SettingsController).
- **provider / base URL / API key / model / temperature / max tokens** → all in `SettingsService` (Task 4) and the QML form (Task 6).
- **字符速率限制器参数 msPerChar (默认 80ms, 40-200ms)** → `SettingsService::setMsPerChar` clamps to that range (Task 4); slider in QML (Task 6); persisted to `chat/msPerChar` QSettings key. Behavior deferred to Phase 2.3.
- **API key 存储策略 macOS / Windows / Linux** → `SecretStore::create()` factory (Task 2); macOS implementation (Task 3); Windows/Linux fall back to `NullSecretStore`.
- **Keychain 不可用兜底 C: env-var only** → `ChatController::startSidecar()` only inserts env vars when `SettingsService` returns non-empty values (Task 7); the parent process env survives, so an externally-set `MILES_PROVIDER_API_KEY` propagates to the sidecar.
- **验收: 普通用户不需要改环境变量即可配置模型** → Task 8 manual test, Step 3.
- **验收: API key 不写入仓库、日志或明文调试输出** → Contract test in Task 1 forbids `provider/apiKey` from `SettingsService.cpp`; the API key is read straight into `QProcessEnvironment` (never logged); the macOS Security framework handles encryption at rest.

### 2. Placeholder scan

Scanning for forbidden patterns: no "TBD" / "implement later" / "Add appropriate error handling" appear. Every code step has either a complete file body or an exact find/replace pair. Steps that change existing code show the before-state literal text.

### 3. Type consistency

- `SettingsService` constructor signature `(SecretStore *secretStore, QObject *parent = nullptr)` — used identically in Task 4 (test fixture), Task 5 (controller), and Task 8 (main.cpp). ✓
- `ChatController` constructor signature `(PetRuntime *runtime, SettingsService *settings, QObject *parent = nullptr)` — used in Task 7 (header), Task 7 (smoke test fixture), Task 8 (main.cpp). ✓
- `SettingsController::saved()` signal name — matches the connect in Task 8 and the `Q_INVOKABLE save()` body in Task 5. ✓
- `ChatController::handleSettingsSaved()` slot — declared in Task 7 header, defined in Task 7 implementation, connected in Task 8 main.cpp. ✓
- QSettings keys `provider/baseUrl`, `provider/model`, `provider/temperature`, `provider/maxTokens`, `chat/msPerChar` — defined in Task 4 `SettingsService.cpp` and asserted in Task 1 contract test. ✓
- Keychain identifiers `dev.tian.MilesEdgeworth.v2` / `MILES_PROVIDER_API_KEY` — defined as `kKeychainService` / `kKeychainAccount` in Task 4 header and referenced in Task 10 stage record. ✓
- `MILES_HAS_MAC_SECRET_STORE` compile definition — set in Task 4 Step 4 (desktop CMake APPLE block) and checked in Task 4 Step 4 (`SecretStore.cpp` factory). ✓

No inconsistencies found.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-22-phase-2-2-settings-and-secret-store.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
