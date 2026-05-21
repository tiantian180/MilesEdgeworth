# Filesystem Skin Packs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Revision notes (review 后修订)**：
>
> - **Task 3 Step 4**：把 "把 JSON 解析主体移到 parseManifestDocument" 拆成 4a/4b/4c 三步，明示"机械剪切、不改 schema 行为"，且每步必须跑全量 ctest 防回归。
> - **Task 5 Step 4**：抽出 `applyManifestState()` 私有方法，构造和 `loadSkinDescriptor` 共用同一套 manifest 衍生初始化（audio definition / facing / movement direction / petSize），并显式清空 recipe / phase / action 残留。避免切皮肤后旧状态泄漏。
> - **Task 7 Step 1 + Step 4**：迁移检查除了"manifest 用 `skin:`"还要验证"每个 `skin:` URL 都在 qrc alias 集合里存在"；迁移脚本改为以 qrc 文件路径为真值源生成完整替换表，不再依赖手写的子集。
> - **Task 8 Step 5**：新增 hot-reload manual smoke（克隆 Miles → 编辑 manifest → 重载 → 验证行为变化），覆盖 Pet Skin Studio 的核心使用场景。
>
> 原始 review 见 Phase 1 收尾对话记录。

**Goal:** Let MilesEdgeworth v2 load skins from ordinary filesystem folders, while keeping the built-in Miles skin bundled and usable without external files.

**Architecture:** Add a thin skin descriptor/discovery layer before `SkinManifestLoader`, resolve portable `skin:` URLs during manifest load, and let `PetRuntime` switch/reload skins through descriptors rather than a hard-coded qrc manifest path. The first implementation keeps Custom Interaction execution in C++ and keeps the existing qrc aliases as compatibility shims.

**Tech Stack:** C++20, Qt 6, Qt Resource System, `QStandardPaths`, `QSettings`, CMake/Ninja, existing Python static checks, existing C++ smoke tests.

---

## Scope Check

This plan implements **Step 4 文件系统皮肤包** only.

Included:
- Built-in and filesystem skin descriptors.
- `skin.json`.
- `skin:` URL resolution.
- User / portable / built-in skin discovery.
- Runtime skin selection and reload.
- Native context menu entries for skin switching.
- Gradual Miles manifest migration from `qrc:/pet` / `qrc:/audio` to `skin:`.

Excluded:
- Phase 2 AI chat, Go sidecar, ChatWindow.
- HitZone image-space schema redesign.
- Continuous scale slider.
- Pet Skin Studio.
- JS/TS Custom Interaction execution.
- Remote skin marketplace, zip import UI, signing, sandboxing.

## File Structure

Create:
- `apps/desktop/src/pet/manifest/SkinDescriptor.h`
  - Small value type for scan-time metadata from `skin.json`.
- `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
  - C++ smoke tests for `skin:` URL resolution, filesystem directory loading, and discovery precedence.
- `tests/check_phase_1_4_skin_package_contract.py`
  - Static contract check for new APIs, qrc layout, `skin.json`, and menu wiring.
- `tests/check_phase_1_4_skin_url_migration.py`
  - Static check that built-in Miles manifest no longer uses `qrc:/pet` or `qrc:/audio`.
- `apps/desktop/resources/skins/miles-edgeworth/skin.json`
  - Built-in Miles skin metadata.

Modify:
- `apps/desktop/src/pet/manifest/SkinManifest.h`
  - Add runtime metadata fields: `skinId`, `skinName`, `skinRootUrl`, `builtin`.
- `apps/desktop/src/pet/manifest/SkinManifestLoader.h`
  - Add descriptor, discovery, directory load, and URL resolution APIs.
- `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
  - Refactor JSON parsing behind shared load helpers; resolve all resource URLs after parsing.
- `apps/desktop/src/pet/PetRuntime.h`
  - Add active-skin properties, available skin list, and reload/switch API.
- `apps/desktop/src/pet/PetRuntime.cpp`
  - Replace hard-coded `:/pet/manifest.json` construction path with descriptor-based loading.
- `apps/desktop/src/pet/interaction/CustomInteractionRegistry.h/.cpp`
  - Add a production-safe `reset()` name if skin reload needs to clear old handlers; keep `clearForTest()` as a wrapper.
- `apps/desktop/src/pet/surface/PetContextMenu.cpp`
  - Add native "皮肤" submenu, "打开皮肤目录", and "重载当前皮肤".
- `apps/desktop/src/main.cpp`
  - Re-register skin-specific custom interactions after runtime skin load/reload.
- `apps/desktop/resources/pet_assets.qrc`
  - Add `/skins/miles-edgeworth` qresource tree while preserving existing `/pet` and `/audio` aliases.
- `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
  - Convert built-in resource references to `skin:assets/...`.
- `apps/desktop/CMakeLists.txt`
  - Add new source/header/test files.
- `CMakeLists.txt`
  - Register new Python static tests if project-level tests are listed there.
- `README.md`
  - Document skin folder locations and reload workflow.
- `docs/v2/设计方案/皮肤包分发与加载机制设计.md`
  - Update "当前代码状态" after implementation.
- `docs/v2/阶段记录/Phase 1 收尾与体验问题修复计划.md`
  - Record Step 4 completion and verification evidence.

## Task 1: Add Skin Descriptor Model and Guardrail Tests

**Files:**
- Create: `apps/desktop/src/pet/manifest/SkinDescriptor.h`
- Create: `tests/check_phase_1_4_skin_package_contract.py`
- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the static contract test**

Create `tests/check_phase_1_4_skin_package_contract.py`:

```python
#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)

loader_h = read("apps/desktop/src/pet/manifest/SkinManifestLoader.h")
manifest_h = read("apps/desktop/src/pet/manifest/SkinManifest.h")
runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
menu_cpp = read("apps/desktop/src/pet/surface/PetContextMenu.cpp")
qrc = read("apps/desktop/resources/pet_assets.qrc")

require("SkinDescriptor.h" in loader_h, "SkinManifestLoader.h must include SkinDescriptor.h")
require("loadFromDirectory" in loader_h, "loader must expose loadFromDirectory()")
require("discoverAll" in loader_h, "loader must expose discoverAll()")
require("resolveSkinUrl" in loader_h, "loader must expose resolveSkinUrl() for tests and deterministic URL handling")
require("userSkinDirectoryPath" in loader_h, "loader must expose userSkinDirectoryPath()")
require("portableSkinDirectoryPath" in loader_h, "loader must expose portableSkinDirectoryPath()")

require("skinId" in manifest_h, "SkinManifest must store loaded skin id")
require("skinName" in manifest_h, "SkinManifest must store loaded skin display name")
require("skinRootUrl" in manifest_h, "SkinManifest must store resolved root URL")
require("builtin" in manifest_h, "SkinManifest must mark built-in skins")

require("availableSkins" in runtime_h, "PetRuntime must expose availableSkins")
require("activeSkinId" in runtime_h, "PetRuntime must expose activeSkinId")
require("setActiveSkin" in runtime_h, "PetRuntime must expose setActiveSkin")
require("reloadActiveSkin" in runtime_h, "PetRuntime must expose reloadActiveSkin")

require("皮肤" in menu_cpp, "context menu must expose a skin submenu")
require("打开皮肤目录" in menu_cpp, "context menu must expose the user skin directory")
require("重载当前皮肤" in menu_cpp, "context menu must expose skin reload")

require('prefix="/skins/miles-edgeworth"' in qrc, "qrc must expose built-in skin under /skins/miles-edgeworth")
require('alias="skin.json"' in qrc, "qrc must include built-in skin.json")
require('alias="manifest.json"' in qrc, "qrc must include built-in manifest.json under skin root")

print("phase 1.4 skin package contract ok")
```

- [ ] **Step 2: Register the static test**

Do not register this test in `CMakeLists.txt` during Task 1. It intentionally checks the full Step 4 contract, so it remains red until runtime/menu/qrc tasks land. Registering it now would make every intermediate `ctest` run fail.

The registration step is moved to Task 6 after the full contract passes.

- [ ] **Step 3: Run the test and verify it fails for the right reason**

Run:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: FAIL mentioning missing `SkinDescriptor.h`, `loadFromDirectory`, or skin menu APIs.

- [ ] **Step 4: Add `SkinDescriptor`**

Create `apps/desktop/src/pet/manifest/SkinDescriptor.h`:

```cpp
#pragma once

#include <QUrl>
#include <QString>

// SkinDescriptor 是扫描阶段使用的轻量元信息。
// 它只来自 skin.json，不解析完整 manifest，避免应用启动时读取所有动作配置。
struct SkinDescriptor
{
    QString id;
    QString name;
    QString version;
    QString author;
    QString license;
    int manifestVersion = 1;
    QString minAppVersion;
    QUrl thumbnailUrl;

    // rootUrl 是资源根目录。内置皮肤形如 qrc:/skins/miles-edgeworth/，
    // 文件系统皮肤形如 file:///.../skins/my-skin/。
    QUrl rootUrl;

    // manifestPath 是 Qt 可直接读取的 manifest.json 位置。
    // 内置皮肤使用 :/skins/miles-edgeworth/manifest.json，
    // 文件系统皮肤使用 /absolute/path/to/skin/manifest.json。
    QString manifestPath;

    bool builtin = false;
};
```

- [ ] **Step 5: Add manifest metadata fields**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, add fields near the top-level manifest metadata:

```cpp
    // 当前加载的皮肤元信息。Runtime 和菜单只读这些字段，不直接读取 skin.json。
    QString skinId;
    QString skinName;
    QUrl skinRootUrl;
    bool builtin = false;
```

- [ ] **Step 6: Declare loader APIs**

Replace the public section of `SkinManifestLoader` with:

```cpp
public:
    static SkinManifest loadFromResource(const QString &resourcePath);
    static SkinManifest loadFromDescriptor(const SkinDescriptor &descriptor);
    static SkinManifest loadFromDirectory(const QString &filesystemPath);
    static SkinManifest fallbackManifest();

    static QList<SkinDescriptor> discoverAll();
    static QList<SkinDescriptor> discoverInDirectories(const QStringList &directories, bool includeBuiltins = true);
    static QString userSkinDirectoryPath();
    static QString portableSkinDirectoryPath();

    static QUrl resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl);
```

Add includes:

```cpp
#include "pet/manifest/SkinDescriptor.h"

#include <QList>
#include <QUrl>
```

- [ ] **Step 7: Run the static test**

Run:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: still FAIL because runtime/menu/qrc are not implemented yet. The descriptor and loader API failures should be gone.

- [ ] **Step 8: Commit Task 1**

```bash
git add apps/desktop/src/pet/manifest/SkinDescriptor.h \
        apps/desktop/src/pet/manifest/SkinManifest.h \
        apps/desktop/src/pet/manifest/SkinManifestLoader.h \
        tests/check_phase_1_4_skin_package_contract.py
git commit -m "feat(skins): add skin descriptor contract"
```

## Task 2: Add Built-In Skin Metadata and qrc Skin Root

**Files:**
- Create: `apps/desktop/resources/skins/miles-edgeworth/skin.json`
- Modify: `apps/desktop/resources/pet_assets.qrc`

- [ ] **Step 1: Add built-in `skin.json`**

Create `apps/desktop/resources/skins/miles-edgeworth/skin.json`:

```json
{
  "id": "miles-edgeworth",
  "name": "御剑怜侍",
  "version": "0.2.0",
  "author": "tiantian180",
  "license": "fan-project",
  "manifestVersion": 1,
  "minAppVersion": "0.2.0",
  "thumbnail": "skin:assets/body/idle/stand-right.gif"
}
```

- [ ] **Step 2: Add `/skins/miles-edgeworth` qresource root**

In `apps/desktop/resources/pet_assets.qrc`, keep the existing `/pet`, `/audio`, and `/icon` sections. Add this new qresource before `</RCC>`:

```xml
  <qresource prefix="/skins/miles-edgeworth">
    <file alias="skin.json">skins/miles-edgeworth/skin.json</file>
    <file alias="manifest.json">skins/miles-edgeworth/manifest.json</file>
    <file alias="assets/body/idle/stand-right.gif">skins/miles-edgeworth/assets/body/idle/stand-right.gif</file>
    <file alias="assets/body/idle/stand-left.gif">skins/miles-edgeworth/assets/body/idle/stand-left.gif</file>
    <file alias="assets/body/startup/briefcase-in-right.gif">skins/miles-edgeworth/assets/body/startup/briefcase-in-right.gif</file>
    <file alias="assets/body/startup/briefcase-stop-right.gif">skins/miles-edgeworth/assets/body/startup/briefcase-stop-right.gif</file>
    <file alias="assets/body/locomotion/walk-east.gif">skins/miles-edgeworth/assets/body/locomotion/walk-east.gif</file>
    <file alias="assets/body/locomotion/walk-west.gif">skins/miles-edgeworth/assets/body/locomotion/walk-west.gif</file>
    <file alias="assets/body/locomotion/walk-north-east.gif">skins/miles-edgeworth/assets/body/locomotion/walk-north-east.gif</file>
    <file alias="assets/body/locomotion/walk-north-west.gif">skins/miles-edgeworth/assets/body/locomotion/walk-north-west.gif</file>
    <file alias="assets/body/locomotion/walk-south-east.gif">skins/miles-edgeworth/assets/body/locomotion/walk-south-east.gif</file>
    <file alias="assets/body/locomotion/walk-south-west.gif">skins/miles-edgeworth/assets/body/locomotion/walk-south-west.gif</file>
    <file alias="assets/body/locomotion/walk-north.gif">skins/miles-edgeworth/assets/body/locomotion/walk-north.gif</file>
    <file alias="assets/body/locomotion/walk-south.gif">skins/miles-edgeworth/assets/body/locomotion/walk-south.gif</file>
    <file alias="assets/body/locomotion/run-east.gif">skins/miles-edgeworth/assets/body/locomotion/run-east.gif</file>
    <file alias="assets/body/locomotion/run-west.gif">skins/miles-edgeworth/assets/body/locomotion/run-west.gif</file>
    <file alias="assets/body/locomotion/run-north-east.gif">skins/miles-edgeworth/assets/body/locomotion/run-north-east.gif</file>
    <file alias="assets/body/locomotion/run-north-west.gif">skins/miles-edgeworth/assets/body/locomotion/run-north-west.gif</file>
    <file alias="assets/body/locomotion/run-south-east.gif">skins/miles-edgeworth/assets/body/locomotion/run-south-east.gif</file>
    <file alias="assets/body/locomotion/run-south-west.gif">skins/miles-edgeworth/assets/body/locomotion/run-south-west.gif</file>
    <file alias="assets/body/locomotion/run-north.gif">skins/miles-edgeworth/assets/body/locomotion/run-north.gif</file>
    <file alias="assets/body/locomotion/run-south.gif">skins/miles-edgeworth/assets/body/locomotion/run-south.gif</file>
    <file alias="assets/body/gestures/thinking-right.gif">skins/miles-edgeworth/assets/body/gestures/thinking-right.gif</file>
    <file alias="assets/body/gestures/thinking-left.gif">skins/miles-edgeworth/assets/body/gestures/thinking-left.gif</file>
    <file alias="assets/body/gestures/tapping-head-right.gif">skins/miles-edgeworth/assets/body/gestures/tapping-head-right.gif</file>
    <file alias="assets/body/gestures/tapping-head-left.gif">skins/miles-edgeworth/assets/body/gestures/tapping-head-left.gif</file>
    <file alias="assets/body/gestures/shrug-right.gif">skins/miles-edgeworth/assets/body/gestures/shrug-right.gif</file>
    <file alias="assets/body/gestures/shrug-left.gif">skins/miles-edgeworth/assets/body/gestures/shrug-left.gif</file>
    <file alias="assets/body/gestures/check-watch-right.gif">skins/miles-edgeworth/assets/body/gestures/check-watch-right.gif</file>
    <file alias="assets/body/gestures/check-watch-left.gif">skins/miles-edgeworth/assets/body/gestures/check-watch-left.gif</file>
    <file alias="assets/body/gestures/pointing-right.gif">skins/miles-edgeworth/assets/body/gestures/pointing-right.gif</file>
    <file alias="assets/body/gestures/pointing-left.gif">skins/miles-edgeworth/assets/body/gestures/pointing-left.gif</file>
    <file alias="assets/body/gestures/sitting-tea-right.gif">skins/miles-edgeworth/assets/body/gestures/sitting-tea-right.gif</file>
    <file alias="assets/body/gestures/sitting-tea-left.gif">skins/miles-edgeworth/assets/body/gestures/sitting-tea-left.gif</file>
    <file alias="assets/body/gestures/phone-call-right.gif">skins/miles-edgeworth/assets/body/gestures/phone-call-right.gif</file>
    <file alias="assets/body/gestures/phone-call-left.gif">skins/miles-edgeworth/assets/body/gestures/phone-call-left.gif</file>
    <file alias="assets/body/gestures/look-back-right.gif">skins/miles-edgeworth/assets/body/gestures/look-back-right.gif</file>
    <file alias="assets/body/gestures/look-back-left.gif">skins/miles-edgeworth/assets/body/gestures/look-back-left.gif</file>
    <file alias="assets/body/gestures/look-down-right.gif">skins/miles-edgeworth/assets/body/gestures/look-down-right.gif</file>
    <file alias="assets/body/gestures/look-down-left.gif">skins/miles-edgeworth/assets/body/gestures/look-down-left.gif</file>
    <file alias="assets/body/gestures/look-up-right.gif">skins/miles-edgeworth/assets/body/gestures/look-up-right.gif</file>
    <file alias="assets/body/gestures/look-up-left.gif">skins/miles-edgeworth/assets/body/gestures/look-up-left.gif</file>
    <file alias="assets/body/interaction/turn-right-to-left.gif">skins/miles-edgeworth/assets/body/interaction/turn-right-to-left.gif</file>
    <file alias="assets/body/interaction/turn-left-to-right.gif">skins/miles-edgeworth/assets/body/interaction/turn-left-to-right.gif</file>
    <file alias="assets/body/interaction/scared-right.gif">skins/miles-edgeworth/assets/body/interaction/scared-right.gif</file>
    <file alias="assets/body/interaction/scared-left.gif">skins/miles-edgeworth/assets/body/interaction/scared-left.gif</file>
    <file alias="assets/body/interaction/back-right.gif">skins/miles-edgeworth/assets/body/interaction/back-right.gif</file>
    <file alias="assets/body/interaction/back-left.gif">skins/miles-edgeworth/assets/body/interaction/back-left.gif</file>
    <file alias="assets/body/interaction/crouch-right.gif">skins/miles-edgeworth/assets/body/interaction/crouch-right.gif</file>
    <file alias="assets/body/interaction/crouch-left.gif">skins/miles-edgeworth/assets/body/interaction/crouch-left.gif</file>
    <file alias="assets/body/interaction/stand-up-full-right.gif">skins/miles-edgeworth/assets/body/interaction/stand-up-full-right.gif</file>
    <file alias="assets/body/interaction/stand-up-full-left.gif">skins/miles-edgeworth/assets/body/interaction/stand-up-full-left.gif</file>
    <file alias="assets/body/interaction/stand-up-quick-right.gif">skins/miles-edgeworth/assets/body/interaction/stand-up-quick-right.gif</file>
    <file alias="assets/body/interaction/stand-up-quick-left.gif">skins/miles-edgeworth/assets/body/interaction/stand-up-quick-left.gif</file>
    <file alias="assets/body/interaction/crossed-right.gif">skins/miles-edgeworth/assets/body/interaction/crossed-right.gif</file>
    <file alias="assets/body/interaction/crossed-left.gif">skins/miles-edgeworth/assets/body/interaction/crossed-left.gif</file>
    <file alias="assets/body/interaction/objecting-right.gif">skins/miles-edgeworth/assets/body/interaction/objecting-right.gif</file>
    <file alias="assets/body/interaction/objecting-left.gif">skins/miles-edgeworth/assets/body/interaction/objecting-left.gif</file>
    <file alias="assets/body/interaction/bow-right.gif">skins/miles-edgeworth/assets/body/interaction/bow-right.gif</file>
    <file alias="assets/body/interaction/bow-left.gif">skins/miles-edgeworth/assets/body/interaction/bow-left.gif</file>
    <file alias="assets/body/interaction/pickup-right.gif">skins/miles-edgeworth/assets/body/interaction/pickup-right.gif</file>
    <file alias="assets/body/interaction/pickup-left.gif">skins/miles-edgeworth/assets/body/interaction/pickup-left.gif</file>
    <file alias="assets/body/menu/tea-right.gif">skins/miles-edgeworth/assets/body/menu/tea-right.gif</file>
    <file alias="assets/body/menu/tea-left.gif">skins/miles-edgeworth/assets/body/menu/tea-left.gif</file>
    <file alias="assets/body/menu/tea-alt-right.gif">skins/miles-edgeworth/assets/body/menu/tea-alt-right.gif</file>
    <file alias="assets/body/menu/tea-alt-left.gif">skins/miles-edgeworth/assets/body/menu/tea-alt-left.gif</file>
    <file alias="assets/body/rest/sleep-right.gif">skins/miles-edgeworth/assets/body/rest/sleep-right.gif</file>
    <file alias="assets/body/rest/sleep-left.gif">skins/miles-edgeworth/assets/body/rest/sleep-left.gif</file>
    <file alias="assets/body/rest/sleeping-right.gif">skins/miles-edgeworth/assets/body/rest/sleeping-right.gif</file>
    <file alias="assets/body/rest/sleeping-left.gif">skins/miles-edgeworth/assets/body/rest/sleeping-left.gif</file>
    <file alias="assets/body/rest/wake-right.gif">skins/miles-edgeworth/assets/body/rest/wake-right.gif</file>
    <file alias="assets/body/rest/wake-left.gif">skins/miles-edgeworth/assets/body/rest/wake-left.gif</file>
    <file alias="assets/props/prosecutor_badge/prosecutor-badge.png">skins/miles-edgeworth/assets/props/prosecutor_badge/prosecutor-badge.png</file>
    <file alias="assets/audio/voice/holdit0.wav">skins/miles-edgeworth/assets/audio/voice/holdit0.wav</file>
    <file alias="assets/audio/voice/holdit1.wav">skins/miles-edgeworth/assets/audio/voice/holdit1.wav</file>
    <file alias="assets/audio/voice/holdit2.wav">skins/miles-edgeworth/assets/audio/voice/holdit2.wav</file>
    <file alias="assets/audio/voice/takethat0.wav">skins/miles-edgeworth/assets/audio/voice/takethat0.wav</file>
    <file alias="assets/audio/voice/takethat1.wav">skins/miles-edgeworth/assets/audio/voice/takethat1.wav</file>
    <file alias="assets/audio/voice/takethat2.wav">skins/miles-edgeworth/assets/audio/voice/takethat2.wav</file>
    <file alias="assets/audio/voice/objection0.wav">skins/miles-edgeworth/assets/audio/voice/objection0.wav</file>
    <file alias="assets/audio/voice/objection1.wav">skins/miles-edgeworth/assets/audio/voice/objection1.wav</file>
    <file alias="assets/audio/voice/objection2.wav">skins/miles-edgeworth/assets/audio/voice/objection2.wav</file>
    <file alias="assets/audio/voice/eureka0.wav">skins/miles-edgeworth/assets/audio/voice/eureka0.wav</file>
    <file alias="assets/audio/voice/eureka1.wav">skins/miles-edgeworth/assets/audio/voice/eureka1.wav</file>
  </qresource>
```

- [ ] **Step 3: Verify qrc contract still fails only on later APIs**

Run:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: qrc and `skin.json` failures are gone; runtime/menu failures remain.

- [ ] **Step 4: Commit Task 2**

```bash
git add apps/desktop/resources/skins/miles-edgeworth/skin.json \
        apps/desktop/resources/pet_assets.qrc
git commit -m "feat(skins): expose built-in skin package"
```

## Task 3: Implement Loader Core, `skin:` URL Resolution, and Filesystem Loading

**Files:**
- Create: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] **Step 1: Write C++ smoke tests**

Create `apps/desktop/tests/skin_manifest_loader_smoke.cpp`:

```cpp
#include "pet/manifest/SkinManifestLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdlib>
#include <iostream>

namespace {
bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << content;
    return true;
}

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QUrl rootUrl(QStringLiteral("file:///tmp/example-skin/"));
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle.gif"), rootUrl).toString()
            == QStringLiteral("file:///tmp/example-skin/assets/body/idle.gif"),
        "skin: URL should resolve under file root"
    );
    require(
        SkinManifestLoader::resolveSkinUrl(QStringLiteral("qrc:/pet/stand-right.gif"), rootUrl).toString()
            == QStringLiteral("qrc:/pet/stand-right.gif"),
        "absolute qrc URL should stay unchanged"
    );
    require(
        !SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:../escape.gif"), rootUrl).isValid(),
        "skin: URL must reject parent traversal"
    );

    QTemporaryDir dir;
    require(dir.isValid(), "temporary skin directory should be valid");
    QDir skinDir(dir.path());
    require(skinDir.mkpath(QStringLiteral("assets/body/idle")), "assets directory should be created");
    require(writeFile(skinDir.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{
  "id": "test-skin",
  "name": "测试皮肤",
  "version": "1.0.0",
  "manifestVersion": 1,
  "thumbnail": "skin:assets/body/idle/stand.gif"
}
)JSON")), "skin.json should be written");
    require(writeFile(skinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "defaultFacing": "right",
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": "skin:assets/body/idle/stand.gif"
      }
    }
  }
}
)JSON")), "manifest.json should be written");

    SkinManifest manifest = SkinManifestLoader::loadFromDirectory(dir.path());
    require(manifest.skinId == QStringLiteral("test-skin"), "loadFromDirectory should populate skinId");
    require(manifest.skinName == QStringLiteral("测试皮肤"), "loadFromDirectory should populate skinName");
    require(!manifest.builtin, "filesystem skin should not be builtin");
    require(manifest.actions.contains(QStringLiteral("idle_stand")), "filesystem manifest should load actions");
    require(
        manifest.actions.value(QStringLiteral("idle_stand")).variants.value(QStringLiteral("right")).toString().startsWith(QStringLiteral("file://")),
        "skin: action URL should resolve to file URL"
    );

    QList<SkinDescriptor> descriptors = SkinManifestLoader::discoverInDirectories(QStringList{dir.path()}, false);
    require(descriptors.size() == 1, "discoverInDirectories should find one test skin");
    require(descriptors.first().id == QStringLiteral("test-skin"), "descriptor id should come from skin.json");
    require(descriptors.first().rootUrl.isLocalFile(), "filesystem descriptor root should be a file URL");

    return 0;
}
```

- [ ] **Step 2: Register the smoke test**

In `apps/desktop/CMakeLists.txt`, add `SkinManifestLoaderSmoke` near existing smoke tests:

```cmake
add_executable(SkinManifestLoaderSmoke
    tests/skin_manifest_loader_smoke.cpp
    src/pet/manifest/SkinManifestLoader.cpp
)

target_link_libraries(SkinManifestLoaderSmoke
    PRIVATE
        Qt6::Core
)

target_include_directories(SkinManifestLoaderSmoke
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(NAME skin_manifest_loader_smoke COMMAND SkinManifestLoaderSmoke)
```

- [ ] **Step 3: Run the smoke test and verify it fails**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: compile or link FAIL because loader methods are declared but not implemented.

- [ ] **Step 4: Refactor loader behind shared helpers**

This refactor is **行为等价**：现有的 JSON 解析逻辑不动一行，只换入口。务必按以下子步骤推进，每一小步都跑 ctest 验证 70+ 个测试仍然通过。

**4a. 添加 LoadContext + 占位 parseManifestDocument 骨架**

In `SkinManifestLoader.cpp`, insert the following near the top of the anonymous namespace (currently around the helper functions like `rectFromJsonObject` / `clickBehaviorEntryFromJsonValue`):

```cpp
namespace {
struct LoadContext
{
    QString sourceName;
    QUrl rootUrl;
    QString skinId;
    QString skinName;
    bool builtin = false;
};

void resolveManifestUrls(SkinManifest &manifest, const QUrl &rootUrl); // 前置声明，Step 6 实现

SkinManifest parseManifestDocument(const QJsonDocument &document, const LoadContext &context);

SkinManifest loadManifestFile(const QString &path, const LoadContext &context)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    return parseManifestDocument(document, context);
}
} // namespace
```

**4b. 把现有 `loadFromResource` 主体原样剪到 `parseManifestDocument`**

当前 `SkinManifestLoader::loadFromResource(const QString &resourcePath)` 包含：
1. 打开文件、parse JSON 部分（开头 ~10 行）
2. 解析 `canvas` / `audio` / `capabilities` / `sizes` / `states` / `actions` / `phases` / `recipes` / `props` / `actionPools` / `expressions` / `expressionMappings` / `behaviorTriggers` / `behaviorRules` / `hitZones` / `skinCommands` / `clickBehaviors` / `customInteractions` / `movementFacingMap` 等大量字段填充（约 400-500 行）
3. `return manifest;`

操作：

- 把 **步骤 2 的全部代码**（从局部变量 `const QJsonObject root = document.object();` 开始，到 `return manifest;` 之前）**整体剪切**到 `parseManifestDocument()` 内
- 在剪过来的代码顶部加：

```cpp
SkinManifest parseManifestDocument(const QJsonDocument &document, const LoadContext &context)
{
    SkinManifest manifest;
    manifest.skinId = context.skinId;
    manifest.skinName = context.skinName;
    manifest.skinRootUrl = context.rootUrl;
    manifest.builtin = context.builtin;

    // ↓↓↓ 从原 loadFromResource() 剪过来的解析代码（不改任何 JSON 字段解析逻辑） ↓↓↓
    const QJsonObject root = document.object();
    // ...（保持原有的 canvas / audio / actions / ... 所有解析）...
    // ↑↑↑ 剪切结束 ↑↑↑

    resolveManifestUrls(manifest, context.rootUrl);
    return manifest;
}
```

- 在剪走代码的位置（即原 `loadFromResource` 内）改为调用新入口（见 Step 7）。**先保留 fallback 检查（`if (m_manifest.actions.isEmpty())`等）的逻辑位置不变**——这些检查应该留在 `loadFromResource` 而不是进 `parseManifestDocument`。

**4c. 增量验证（不可省略）**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

期望：全部 70+ 个测试仍然通过。如果有任何一条失败，先回滚 4b 并定位漏剪 / 多剪的代码，再重做。这一步是回归保护，**不能合并到后续 Step**。

> ⚠️ 注意：这步是机械性 cut-paste，**任何 schema 行为变化（即使是"看起来更合理"的修正）都不允许**。如果你发现原代码有 bug，请记下来留到后续 commit 单独修。

- [ ] **Step 5: Implement `resolveSkinUrl`**

Add this public method implementation:

```cpp
QUrl SkinManifestLoader::resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl)
{
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.scheme() != QStringLiteral("skin")) {
        return url;
    }

    QString relativePath = trimmed.mid(QStringLiteral("skin:").size());
    while (relativePath.startsWith(QLatin1Char('/'))) {
        relativePath.remove(0, 1);
    }
    if (relativePath.isEmpty() || relativePath.contains(QStringLiteral(".."))) {
        return {};
    }

    QUrl resolved = rootUrl;
    QString base = resolved.path();
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    resolved.setPath(base + relativePath);
    return resolved;
}
```

- [ ] **Step 6: Implement manifest URL rewriting**

Add a private helper in `SkinManifestLoader.cpp`:

```cpp
void resolveManifestUrls(SkinManifest &manifest, const QUrl &rootUrl)
{
    for (ActionDefinition &action : manifest.actions) {
        for (QUrl &url : action.variants) {
            url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
        }
        for (PhaseDefinition &phase : action.phases) {
            for (QUrl &url : phase.variants) {
                url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
            }
        }
    }

    for (PropDefinition &prop : manifest.props) {
        prop.assetUrl = SkinManifestLoader::resolveSkinUrl(prop.assetUrl.toString(), rootUrl);
    }

    for (RecipeDefinition &recipe : manifest.recipes) {
        recipe.soundUrl = SkinManifestLoader::resolveSkinUrl(recipe.soundUrl.toString(), rootUrl);
        for (QUrl &url : recipe.soundUrls) {
            url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
        }
    }
}
```

This helper intentionally only touches URL-bearing fields in the current `SkinManifest.h`: action variants, phase variants, prop assets, and recipe sound URLs.

- [ ] **Step 7: Implement resource and directory loading**

Implement:

```cpp
SkinManifest SkinManifestLoader::loadFromResource(const QString &resourcePath)
{
    return loadManifestFile(resourcePath, LoadContext{
        .sourceName = resourcePath,
        .rootUrl = QUrl(QStringLiteral("qrc:/skins/miles-edgeworth/")),
        .skinId = QStringLiteral("miles-edgeworth"),
        .skinName = QStringLiteral("御剑怜侍"),
        .builtin = true,
    });
}

SkinManifest SkinManifestLoader::loadFromDescriptor(const SkinDescriptor &descriptor)
{
    SkinManifest manifest = loadManifestFile(descriptor.manifestPath, LoadContext{
        .sourceName = descriptor.manifestPath,
        .rootUrl = descriptor.rootUrl,
        .skinId = descriptor.id,
        .skinName = descriptor.name,
        .builtin = descriptor.builtin,
    });
    if (manifest.skinName.isEmpty()) {
        manifest.skinName = descriptor.id;
    }
    return manifest;
}

SkinManifest SkinManifestLoader::loadFromDirectory(const QString &filesystemPath)
{
    QList<SkinDescriptor> descriptors = discoverInDirectories(QStringList{filesystemPath}, false);
    if (descriptors.isEmpty()) {
        return {};
    }
    return loadFromDescriptor(descriptors.first());
}
```

- [ ] **Step 8: Run loader smoke**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: PASS.

- [ ] **Step 9: Commit Task 3**

```bash
git add apps/desktop/src/pet/manifest/SkinManifestLoader.cpp \
        apps/desktop/tests/skin_manifest_loader_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "feat(skins): load filesystem skin manifests"
```

## Task 4: Implement Skin Discovery and Precedence

**Files:**
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`

- [ ] **Step 1: Extend smoke test for precedence**

Append to `skin_manifest_loader_smoke.cpp` after the first descriptor assertions:

```cpp
QTemporaryDir userDir;
QTemporaryDir portableDir;
require(userDir.isValid() && portableDir.isValid(), "precedence dirs should be valid");
QDir userSkin(userDir.path() + QStringLiteral("/miles-edgeworth"));
QDir portableSkin(portableDir.path() + QStringLiteral("/miles-edgeworth"));
require(userSkin.mkpath(QStringLiteral(".")), "user skin dir should be created");
require(portableSkin.mkpath(QStringLiteral(".")), "portable skin dir should be created");
require(writeFile(userSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"same-id","name":"用户版本","version":"1.0.0","manifestVersion":1}
)JSON")), "user skin.json should be written");
require(writeFile(portableSkin.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"same-id","name":"便携版本","version":"1.0.0","manifestVersion":1}
)JSON")), "portable skin.json should be written");
require(writeFile(userSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")), "user manifest should be written");
require(writeFile(portableSkin.filePath(QStringLiteral("manifest.json")), QStringLiteral("{}")), "portable manifest should be written");

QList<SkinDescriptor> precedence = SkinManifestLoader::discoverInDirectories(
    QStringList{userDir.path(), portableDir.path()},
    false
);
require(precedence.size() == 1, "same id should be deduplicated");
require(precedence.first().name == QStringLiteral("用户版本"), "earlier directory should win on duplicate skin id");
```

- [ ] **Step 2: Run smoke and verify failure**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: FAIL until discovery is implemented.

- [ ] **Step 3: Implement descriptor parsing**

Add helpers to `SkinManifestLoader.cpp`:

```cpp
SkinDescriptor descriptorFromSkinJson(
    const QString &skinJsonPath,
    const QUrl &rootUrl,
    bool builtin
)
{
    QFile file(skinJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    SkinDescriptor descriptor;
    descriptor.id = object.value(QStringLiteral("id")).toString().trimmed();
    descriptor.name = object.value(QStringLiteral("name")).toString().trimmed();
    descriptor.version = object.value(QStringLiteral("version")).toString().trimmed();
    descriptor.author = object.value(QStringLiteral("author")).toString().trimmed();
    descriptor.license = object.value(QStringLiteral("license")).toString().trimmed();
    descriptor.manifestVersion = object.value(QStringLiteral("manifestVersion")).toInt(1);
    descriptor.minAppVersion = object.value(QStringLiteral("minAppVersion")).toString().trimmed();
    descriptor.rootUrl = rootUrl;
    descriptor.builtin = builtin;
    descriptor.thumbnailUrl = SkinManifestLoader::resolveSkinUrl(
        object.value(QStringLiteral("thumbnail")).toString(),
        rootUrl
    );

    if (descriptor.id.isEmpty()) {
        return {};
    }
    if (descriptor.name.isEmpty()) {
        descriptor.name = descriptor.id;
    }
    return descriptor;
}
```

- [ ] **Step 4: Implement directory discovery**

Implement:

```cpp
QList<SkinDescriptor> SkinManifestLoader::discoverInDirectories(const QStringList &directories, bool includeBuiltins)
{
    QList<SkinDescriptor> result;
    QSet<QString> seenIds;

    auto appendDescriptor = [&](SkinDescriptor descriptor) {
        if (descriptor.id.isEmpty() || seenIds.contains(descriptor.id)) {
            return;
        }
        seenIds.insert(descriptor.id);
        result.append(std::move(descriptor));
    };

    for (const QString &directoryPath : directories) {
        QDir root(directoryPath);
        if (!root.exists()) {
            continue;
        }

        const QFileInfo rootSkinJson(root.filePath(QStringLiteral("skin.json")));
        if (rootSkinJson.exists()) {
            SkinDescriptor descriptor = descriptorFromSkinJson(
                rootSkinJson.absoluteFilePath(),
                QUrl::fromLocalFile(root.absolutePath() + QLatin1Char('/')),
                false
            );
            descriptor.manifestPath = root.filePath(QStringLiteral("manifest.json"));
            appendDescriptor(std::move(descriptor));
            continue;
        }

        const QFileInfoList children = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &child : children) {
            const QString skinJsonPath = QDir(child.absoluteFilePath()).filePath(QStringLiteral("skin.json"));
            if (!QFileInfo::exists(skinJsonPath)) {
                continue;
            }
            SkinDescriptor descriptor = descriptorFromSkinJson(
                skinJsonPath,
                QUrl::fromLocalFile(child.absoluteFilePath() + QLatin1Char('/')),
                false
            );
            descriptor.manifestPath = QDir(child.absoluteFilePath()).filePath(QStringLiteral("manifest.json"));
            appendDescriptor(std::move(descriptor));
        }
    }

    if (includeBuiltins) {
        SkinDescriptor miles;
        miles.id = QStringLiteral("miles-edgeworth");
        miles.name = QStringLiteral("御剑怜侍");
        miles.version = QStringLiteral("0.2.0");
        miles.manifestVersion = 1;
        miles.rootUrl = QUrl(QStringLiteral("qrc:/skins/miles-edgeworth/"));
        miles.thumbnailUrl = SkinManifestLoader::resolveSkinUrl(QStringLiteral("skin:assets/body/idle/stand-right.gif"), miles.rootUrl);
        miles.manifestPath = QStringLiteral(":/skins/miles-edgeworth/manifest.json");
        miles.builtin = true;
        appendDescriptor(std::move(miles));
    }

    return result;
}
```

- [ ] **Step 5: Implement standard paths**

Add:

```cpp
QString SkinManifestLoader::userSkinDirectoryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/skins");
}

QString SkinManifestLoader::portableSkinDirectoryPath()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/skins");
}

QList<SkinDescriptor> SkinManifestLoader::discoverAll()
{
    return discoverInDirectories(
        QStringList{userSkinDirectoryPath(), portableSkinDirectoryPath()},
        true
    );
}
```

Add required includes:

```cpp
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
```

- [ ] **Step 6: Run loader smoke**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke
ctest --test-dir build --output-on-failure -R skin_manifest_loader_smoke
```

Expected: PASS.

- [ ] **Step 7: Commit Task 4**

```bash
git add apps/desktop/src/pet/manifest/SkinManifestLoader.cpp \
        apps/desktop/tests/skin_manifest_loader_smoke.cpp
git commit -m "feat(skins): discover installed skin packages"
```

## Task 5: Add Runtime Skin Switching and Reload

**Files:**
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/interaction/CustomInteractionRegistry.h`
- Modify: `apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Add runtime API tests**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, add assertions after constructing `PetRuntime runtime;`:

```cpp
require(!runtime.activeSkinId().isEmpty(), "runtime should have an active skin id");
require(runtime.activeSkinId() == QStringLiteral("miles-edgeworth"), "built-in Miles should be active by default");
require(!runtime.availableSkins().isEmpty(), "runtime should expose available skins");
require(runtime.reloadActiveSkin(), "runtime should reload active skin");
```

- [ ] **Step 2: Run smoke and verify failure**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: compile FAIL because runtime APIs are missing.

- [ ] **Step 3: Add runtime properties and methods**

In `PetRuntime.h`, add properties:

```cpp
Q_PROPERTY(QString activeSkinId READ activeSkinId NOTIFY activeSkinChanged)
Q_PROPERTY(QVariantList availableSkins READ availableSkins NOTIFY availableSkinsChanged)
```

Add public methods:

```cpp
QString activeSkinId() const;
QVariantList availableSkins() const;
Q_INVOKABLE bool setActiveSkin(const QString &skinId);
Q_INVOKABLE bool reloadActiveSkin();
```

Add signals:

```cpp
void activeSkinChanged();
void availableSkinsChanged();
void skinManifestReloaded();
```

Add private members:

```cpp
QList<SkinDescriptor> m_availableSkinDescriptors;
QString m_activeSkinId;

bool loadSkinDescriptor(const SkinDescriptor &descriptor);
SkinDescriptor descriptorForSkinId(const QString &skinId) const;
void refreshAvailableSkins();
```

- [ ] **Step 4: Implement runtime loading**

> ⚠️ **关键约束**：当前 `PetRuntime` 构造里有一段 manifest 加载完成之后的初始化（设置 audio definition、`m_currentFacing`、`m_currentMovementDirection`、`setPetSize` 等）。切皮肤时必须**复用同一套初始化**，否则旧皮肤的 audio 语言列表、petSize、movement direction 会残留。本步骤把这套初始化抽到一个 `applyManifestState()` 私有方法，构造和 `loadSkinDescriptor` 都用它。

**4a. 抽出 `applyManifestState()`**

在 `PetRuntime.h` 私有方法块加：

```cpp
void applyManifestState();
```

在 `PetRuntime.cpp` 内新增：

```cpp
void PetRuntime::applyManifestState()
{
    // manifest 切换后必须做的全部初始化。新增 manifest 衍生字段时也加到这里。
    m_audioController.setAudioDefinition(m_manifest.audio);

    m_currentFacing = m_manifest.defaultFacing.isEmpty()
        ? QStringLiteral("right")
        : m_manifest.defaultFacing;
    m_currentMovementDirection = m_manifest.movementDirections.isEmpty()
        ? QString()
        : m_manifest.movementDirections.constFirst();

    setPetSize(m_manifest.defaultSizeId);
}
```

**4b. 构造里替换为复用调用**

把当前构造里 manifest 加载之后的初始化代码删掉，替换成：

```cpp
PetRuntime::PetRuntime(QObject *parent)
    : QObject(parent)
{
    connect(&m_propController, &PropController::currentPropChanged, this, &PetRuntime::currentPropChanged);
    connect(&m_propController, &PropController::currentPropPlaybackSerialChanged, this, &PetRuntime::currentPropPlaybackSerialChanged);

    refreshAvailableSkins();
    const QString savedSkinId = QSettings()
        .value(QStringLiteral("skin/activeSkinId"), QStringLiteral("miles-edgeworth"))
        .toString();

    if (!setActiveSkin(savedSkinId)
            && !setActiveSkin(QStringLiteral("miles-edgeworth"))) {
        // 内置 Miles 也加载失败时走兜底 manifest，确保桌宠至少能站起来。
        m_manifest = SkinManifestLoader::fallbackManifest();
        m_activeSkinId = QStringLiteral("miles-edgeworth");
        applyManifestState();
        setState(QStringLiteral("idle"));
        startStartupSequence();
    }
}
```

**4c. 实现 discovery / lookup / load / set / reload**

```cpp
void PetRuntime::refreshAvailableSkins()
{
    m_availableSkinDescriptors = SkinManifestLoader::discoverAll();
    emit availableSkinsChanged();
}

SkinDescriptor PetRuntime::descriptorForSkinId(const QString &skinId) const
{
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        if (descriptor.id == skinId) {
            return descriptor;
        }
    }
    return {};
}

bool PetRuntime::loadSkinDescriptor(const SkinDescriptor &descriptor)
{
    if (descriptor.id.isEmpty()) {
        return false;
    }

    SkinManifest nextManifest = SkinManifestLoader::loadFromDescriptor(descriptor);
    if (nextManifest.actions.isEmpty()) {
        return false;
    }

    // 切换皮肤前彻底清掉旧 manifest 衍生的运行时状态，避免 recipe / phase / action 残留。
    hideCurrentProp();
    clearActiveRecipe();
    m_currentActionId.clear();
    m_currentPhaseId.clear();
    m_currentMovementDirection.clear();
    m_currentLoopMode = QStringLiteral("loop");
    m_currentAutoReturnToIdle = false;
    m_currentAnimationUrl.clear();
    ++m_playbackSerial;
    emit playbackSerialChanged();

    m_manifest = std::move(nextManifest);
    m_activeSkinId = descriptor.id;

    applyManifestState();
    setState(QStringLiteral("idle"));
    emit activeSkinChanged();
    emit skinManifestReloaded();
    startStartupSequence();
    return true;
}

bool PetRuntime::setActiveSkin(const QString &skinId)
{
    // 仅在描述符列表为空时刷新，避免构造里和这里各扫一次盘。
    if (m_availableSkinDescriptors.isEmpty()) {
        refreshAvailableSkins();
    }
    SkinDescriptor descriptor = descriptorForSkinId(skinId);
    if (descriptor.id.isEmpty()) {
        // 列表可能过时，再刷一次确认。
        refreshAvailableSkins();
        descriptor = descriptorForSkinId(skinId);
    }
    if (!loadSkinDescriptor(descriptor)) {
        return false;
    }
    QSettings().setValue(QStringLiteral("skin/activeSkinId"), m_activeSkinId);
    return true;
}

bool PetRuntime::reloadActiveSkin()
{
    refreshAvailableSkins();
    return loadSkinDescriptor(descriptorForSkinId(m_activeSkinId));
}
```

> 📌 **如果 `PetRuntime` 现有 `clearActiveRecipe()` / `hideCurrentProp()` 名字不同，沿用现有名字，不要自己造新的**。Step 4 的目的是复用，不是重写。

- [ ] **Step 5: Implement `availableSkins()`**

```cpp
QVariantList PetRuntime::availableSkins() const
{
    QVariantList skins;
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        QVariantMap skin;
        skin.insert(QStringLiteral("id"), descriptor.id);
        skin.insert(QStringLiteral("name"), descriptor.name);
        skin.insert(QStringLiteral("version"), descriptor.version);
        skin.insert(QStringLiteral("builtin"), descriptor.builtin);
        skin.insert(QStringLiteral("thumbnailUrl"), descriptor.thumbnailUrl.toString());
        skins.append(skin);
    }
    return skins;
}
```

- [ ] **Step 6: Add production registry reset**

In `CustomInteractionRegistry.h`, add:

```cpp
static void reset();
```

In `.cpp`, rename the current clear implementation to `reset()` and make `clearForTest()` call it:

```cpp
void CustomInteractionRegistry::reset()
{
    registeredInteractions().clear();
    interactionStates().clear();
}

void CustomInteractionRegistry::clearForTest()
{
    reset();
}
```

- [ ] **Step 7: Re-register interactions after skin reload**

In `main.cpp`, create one registration lambda after `PetRuntime petRuntime;`:

```cpp
auto registerCurrentSkinInteractions = [&petRuntime]() {
    CustomInteractionRegistry::reset();
    CustomInteractionRegistry::registerBuiltins(petRuntime.manifest());
    registerMilesEdgeworthInteractions(petRuntime.manifest());
};
registerCurrentSkinInteractions();
QObject::connect(&petRuntime, &PetRuntime::skinManifestReloaded, &app, registerCurrentSkinInteractions);
```

Remove the old one-shot registration lines so registration happens through this lambda only.

- [ ] **Step 8: Run runtime smoke**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: PASS.

- [ ] **Step 9: Run static contract**

Run:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: still FAIL only on menu wiring if Task 6 has not run yet.

- [ ] **Step 10: Commit Task 5**

```bash
git add apps/desktop/src/pet/PetRuntime.h \
        apps/desktop/src/pet/PetRuntime.cpp \
        apps/desktop/src/pet/interaction/CustomInteractionRegistry.h \
        apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp \
        apps/desktop/src/main.cpp \
        apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "feat(skins): switch and reload active skin"
```

## Task 6: Add Native Skin Menu

**Files:**
- Modify: `apps/desktop/src/pet/surface/PetContextMenu.cpp`

- [ ] **Step 1: Add required includes**

Add:

```cpp
#include "pet/manifest/SkinManifestLoader.h"

#include <QDesktopServices>
#include <QDir>
#include <QUrl>
```

- [ ] **Step 2: Add skin submenu before skin commands**

In `PetContextMenu::show()`, add this block before the existing "皮肤动作" menu:

```cpp
const QVariantList skins = runtime->availableSkins();
if (!skins.isEmpty()) {
    menu.addSeparator();
    QMenu *skinMenu = menu.addMenu(QStringLiteral("皮肤"));
    auto *skinGroup = new QActionGroup(skinMenu);
    skinGroup->setExclusive(true);

    for (const QVariant &skinValue : skins) {
        const QVariantMap skin = skinValue.toMap();
        const QString skinId = skin.value(QStringLiteral("id")).toString();
        const QString skinName = skin.value(QStringLiteral("name")).toString();
        if (skinId.isEmpty()) {
            continue;
        }

        QAction *skinAction = addCheckedAction(
            skinMenu,
            skinGroup,
            skinName.isEmpty() ? skinId : skinName,
            runtime->activeSkinId() == skinId
        );
        QObject::connect(skinAction, &QAction::triggered, parent, [runtime, skinId]() {
            runtime->setActiveSkin(skinId);
        });
    }

    skinMenu->addSeparator();
    QAction *reloadSkinAction = skinMenu->addAction(QStringLiteral("重载当前皮肤"));
    QObject::connect(reloadSkinAction, &QAction::triggered, runtime, &PetRuntime::reloadActiveSkin);

    QAction *openSkinDirectoryAction = skinMenu->addAction(QStringLiteral("打开皮肤目录"));
    QObject::connect(openSkinDirectoryAction, &QAction::triggered, parent, []() {
        const QString path = SkinManifestLoader::userSkinDirectoryPath();
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
}
```

- [ ] **Step 3: Run static contract**

Run:

```bash
python3 tests/check_phase_1_4_skin_package_contract.py
```

Expected: PASS.

- [ ] **Step 4: Register the static contract in CMake**

Add this to the root `CMakeLists.txt` test section near the other Python phase checks:

```cmake
add_test(
    NAME check_phase_1_4_skin_package_contract
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_1_4_skin_package_contract.py
)
```

- [ ] **Step 5: Build**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R "skin_manifest_loader_smoke|pet_runtime_smoke|check_phase_1_4_skin_package_contract"
```

Expected: PASS.

- [ ] **Step 6: Commit Task 6**

```bash
git add apps/desktop/src/pet/surface/PetContextMenu.cpp CMakeLists.txt
git commit -m "feat(skins): add skin selection menu"
```

## Task 7: Migrate Miles Manifest to `skin:` URLs

**Files:**
- Create: `tests/check_phase_1_4_skin_url_migration.py`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write migration check**

> ⚠️ **关键约束**：这个测试除了正面检查"manifest 用了 `skin:`"，还**必须验证 manifest 引用的每个 `skin:` URL 都能在 qrc 中找到对应文件**。否则一旦迁移脚本漏改子目录（比如 `qrc:/pet/scared-right.gif` 错误地变成 `skin:assets/body/scared-right.gif` 而不是 `skin:assets/body/interaction/scared-right.gif`），manifest 检查会过，但 runtime 加载时报"file not found"。

Create `tests/check_phase_1_4_skin_url_migration.py`:

```python
#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
qrc_path = ROOT / "apps/desktop/resources/pet_assets.qrc"
raw = manifest_path.read_text(encoding="utf-8")

if "qrc:/pet/" in raw or "qrc:/audio/" in raw:
    raise AssertionError("built-in Miles manifest should use skin: URLs, not qrc:/pet or qrc:/audio")

manifest = json.loads(raw)

def walk(value):
    if isinstance(value, dict):
        for item in value.values():
            yield from walk(item)
    elif isinstance(value, list):
        for item in value:
            yield from walk(item)
    elif isinstance(value, str):
        yield value

skin_urls = [value for value in walk(manifest) if value.startswith("skin:")]
if not skin_urls:
    raise AssertionError("manifest should contain skin: URLs")

required = [
    "skin:assets/body/idle/stand-right.gif",
    "skin:assets/body/locomotion/walk-east.gif",
    "skin:assets/body/locomotion/run-east.gif",
    "skin:assets/props/prosecutor_badge/prosecutor-badge.png",
    "skin:assets/audio/voice/holdit0.wav",
]
missing = [url for url in required if url not in skin_urls]
if missing:
    raise AssertionError(f"missing required skin URLs: {missing}")

# 从 qrc 提取 /skins/miles-edgeworth 下所有 alias 集合
qrc_text = qrc_path.read_text(encoding="utf-8")
skin_section = re.search(
    r'<qresource\s+prefix="/skins/miles-edgeworth">(.*?)</qresource>',
    qrc_text,
    re.S,
)
if skin_section is None:
    raise AssertionError("qrc must expose /skins/miles-edgeworth resource block")

aliases = set(re.findall(r'alias="([^"]+)"', skin_section.group(1)))

# 逐条验证 manifest 内的 skin: URL 都在 qrc 中存在
unknown = []
for url in skin_urls:
    # skin:assets/body/foo.gif -> assets/body/foo.gif
    relative = url[len("skin:"):].lstrip("/")
    if relative not in aliases:
        unknown.append(url)

if unknown:
    raise AssertionError(
        "skin: URLs not present in /skins/miles-edgeworth qrc aliases:\n  "
        + "\n  ".join(sorted(unknown))
    )

print("phase 1.4 skin url migration ok")
```

- [ ] **Step 2: Register migration check**

Add this test to root `CMakeLists.txt`:

```cmake
add_test(
    NAME phase_1_4_skin_url_migration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_phase_1_4_skin_url_migration.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

- [ ] **Step 3: Run and verify failure**

Run:

```bash
python3 tests/check_phase_1_4_skin_url_migration.py
```

Expected: FAIL because current manifest still contains `qrc:/pet` or `qrc:/audio`.

- [ ] **Step 4: Replace manifest URLs**

> ⚠️ **关键约束**：当前 manifest 引用了 ~70 个 `qrc:/pet/` 文件名，分布在 8 个子目录里（idle / startup / locomotion / gestures / interaction / menu / rest / props）。简单的 `qrc:/pet/` → `skin:assets/body/` 替换会让所有非顶层文件指向错误路径。
>
> 正确做法是**用 qrc 文件的真实路径作为真值源**，按 alias 生成完整 mapping。

**4a. 从 qrc 生成完整替换表**

从 repo 根目录运行：

```bash
python3 - <<'PY'
import re
from pathlib import Path

qrc_path = Path("apps/desktop/resources/pet_assets.qrc")
qrc_text = qrc_path.read_text(encoding="utf-8")

# 从 /skins/miles-edgeworth qresource 中提取 alias → 真实路径
section = re.search(
    r'<qresource\s+prefix="/skins/miles-edgeworth">(.*?)</qresource>',
    qrc_text,
    re.S,
)
if section is None:
    raise SystemExit("must run Task 2 first to add /skins/miles-edgeworth qresource block")

aliases = re.findall(
    r'<file\s+alias="([^"]+)">([^<]+)</file>',
    section.group(1),
)

# 当前 manifest 用 qrc:/pet/foo.gif 引用，我们要把它替换成
# skin:<alias>，其中 <alias> 是 /skins/miles-edgeworth/ 下与该文件名匹配的 alias。
# 例如 qrc:/pet/scared-right.gif -> assets/body/interaction/scared-right.gif
qrc_pet_replacements = {}
qrc_audio_replacements = {}

for alias, source in aliases:
    filename = Path(alias).name
    # /pet 子集：所有 GIF/PNG 都在 /pet 下，按文件名匹配
    if alias.startswith("assets/body/") or alias.startswith("assets/props/"):
        qrc_pet_replacements[f"qrc:/pet/{filename}"] = f"skin:{alias}"
    # /audio 子集：voice 音频
    if alias.startswith("assets/audio/"):
        qrc_audio_replacements[f"qrc:/audio/{filename}"] = f"skin:{alias}"

manifest_path = Path("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
text = manifest_path.read_text(encoding="utf-8")

# 按从长到短排序，避免 prefix 误替换（虽然实际 alias 名都互不前缀，但稳妥）
for old, new in sorted({**qrc_pet_replacements, **qrc_audio_replacements}.items(), key=lambda p: -len(p[0])):
    text = text.replace(old, new)

manifest_path.write_text(text, encoding="utf-8")

# 报告剩余的 qrc:/ 引用（如果有）
remaining = [line for line in text.splitlines() if "qrc:/" in line]
if remaining:
    print("WARNING - remaining qrc: references:")
    for line in remaining:
        print("  " + line.strip())
else:
    print("ok - manifest fully migrated to skin: URLs")
PY
```

> ⚠️ 如果脚本报告 `WARNING - remaining qrc:`，说明 manifest 引用了一些 qrc 别名（如 `idle-thinking-once-right.gif` 这种冗余 alias）在 `/skins/miles-edgeworth` qrc 块里没声明。**这种情况要在 Task 2 的 qrc 里补 alias**，或者改 manifest 直接引用规范名（`thinking-right.gif`）。两个方向都行，但必须解决后再继续。

**4b. 检查 diff 是否只动了 URL**

```bash
git diff -- apps/desktop/resources/skins/miles-edgeworth/manifest.json
```

期望：只有以 `qrc:` 或 `skin:` 开头的字符串变化；action id、pool id、hit zone 坐标、behavior 值等所有非 URL 字段保持不变。

如果看到任何 action id 变化、weight 变化、坐标变化，**立刻回滚**——脚本出 bug 了。

- [ ] **Step 5: Run migration and smoke tests**

Run:

```bash
python3 tests/check_phase_1_4_skin_url_migration.py
cmake --build build
ctest --test-dir build --output-on-failure -R "skin_manifest_loader_smoke|pet_runtime_smoke|phase_1_4_skin_url_migration"
```

Expected: PASS.

- [ ] **Step 6: Commit Task 7**

```bash
git add apps/desktop/resources/skins/miles-edgeworth/manifest.json \
        tests/check_phase_1_4_skin_url_migration.py \
        CMakeLists.txt
git commit -m "refactor(skins): migrate miles manifest to skin urls"
```

## Task 8: Documentation, Manual Verification, and Final Checks

**Files:**
- Modify: `README.md`
- Modify: `docs/v2/设计方案/皮肤包分发与加载机制设计.md`
- Modify: `docs/v2/阶段记录/Phase 1 收尾与体验问题修复计划.md`

- [ ] **Step 1: Update README skin section**

Add a concise section:

````markdown
### 皮肤包加载

v2 支持从文件系统加载皮肤包。皮肤包是一个普通目录，至少包含：

```text
my-skin/
  skin.json
  manifest.json
  assets/
```

右键桌宠 → `皮肤` → `打开皮肤目录` 可以打开当前用户皮肤目录。把皮肤目录放进去后，选择 `重载当前皮肤` 或重启应用即可重新扫描。

内置 Miles 皮肤仍打包在应用内；文件系统里出现同 id 皮肤时，用户皮肤优先。
````

- [ ] **Step 2: Update design doc status**

In `docs/v2/设计方案/皮肤包分发与加载机制设计.md`, replace the current code-status block with:

```markdown
> **当前代码状态（Phase 1 Step 4 后）**：运行时已经支持扫描用户皮肤目录、便携皮肤目录和内置 Miles 皮肤；manifest 内的 `skin:` URL 会在加载时解析为 `file://` 或 `qrc:/skins/...`。旧 `/pet` 与 `/audio` qrc alias 暂时保留，用于兼容历史 release 和排查资源问题。
```

同时把设计文档中的内置皮肤 `name` 字段示例由「御剑怀宇」改为「御剑怜侍」，和实际 `skin.json` / 菜单显示保持一致：

```bash
# 检查 inconsistency 是否还存在
grep -n "御剑怀宇" docs/v2/设计方案/皮肤包分发与加载机制设计.md
# 期望：无输出（已改完）
```

- [ ] **Step 3: Update phase record**

Append to `docs/v2/阶段记录/Phase 1 收尾与体验问题修复计划.md`:

````markdown
## Step 4 实施记录：文件系统皮肤包

- 新增 `skin.json` 元信息层。
- 新增 `SkinDescriptor`、`loadFromDirectory()`、`discoverAll()` 和 `skin:` URL 解析。
- 内置 Miles 通过 `qrc:/skins/miles-edgeworth/` 暴露，同时保留历史 `qrc:/pet` / `qrc:/audio` alias。
- 右键菜单新增 `皮肤` 子菜单，支持切换皮肤、重载当前皮肤、打开用户皮肤目录。
- Miles manifest 已迁移到 `skin:assets/...`，文件系统皮肤和内置皮肤共用同一套 manifest URL 写法。

验证命令：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```
````

- [ ] **Step 4: Run full verification**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

Expected: all pass.

- [ ] **Step 5: Manual app smoke**

Run:

```bash
pkill -f MilesEdgeworthDesktop || true
open build/apps/desktop/MilesEdgeworthDesktop.app
sleep 2
pgrep -fl MilesEdgeworthDesktop
```

Manual checks:
- Right-click menu has native "皮肤" submenu.
- Built-in "御剑怜侍" is checked.
- "打开皮肤目录" opens a filesystem directory.
- "重载当前皮肤" keeps the app running and returns to idle/startup behavior.
- Existing standing, idle random, walking/running, single click, double click, sleep, tea, and badge behavior still work.

**Hot-reload smoke**（Studio 集成的核心场景，必须验证）：

1. 用文件管理器打开用户皮肤目录（菜单"打开皮肤目录"）
2. 把内置 Miles 整套资源复制到 `<user-skins>/miles-edgeworth-copy/`，并把里面的 `skin.json` 的 `id` 改成 `miles-edgeworth-copy`、`name` 改成"Miles 副本"
3. 在桌宠菜单"皮肤"下应该出现"Miles 副本"，点击切换
4. 编辑 `<user-skins>/miles-edgeworth-copy/manifest.json`，比如把某个 hit zone 坐标改一个明显的偏移
5. 在桌宠菜单点击"重载当前皮肤"
6. 单击原来命中区域、现在应该不命中的位置，确认行为按新 manifest 表现

如果重载没生效，可能是 `loadSkinDescriptor` 没清干净 m_currentActionId 或 cache 了旧 manifest 引用——回到 Task 5 Step 4 检查。

Stop app:

```bash
pkill -f MilesEdgeworthDesktop || true
```

- [ ] **Step 6: Commit Task 8**

```bash
git add README.md \
        docs/v2/设计方案/皮肤包分发与加载机制设计.md \
        docs/v2/阶段记录/Phase\ 1\ 收尾与体验问题修复计划.md
git commit -m "docs(skins): document filesystem skin packages"
```

## Final Validation Before Handoff

Run:

```bash
git status --short
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

Expected:
- No uncommitted files except user-owned unrelated docs if the user edited them during execution.
- Build passes.
- All tests pass.
- No whitespace errors.

## Review Checklist

- `PetRuntime` no longer hard-codes `:/pet/manifest.json` as the only load path.
- Built-in Miles still works without any external skin directory.
- Filesystem skin package can be discovered from a temporary directory in smoke tests.
- `skin:` URL resolution rejects parent traversal.
- User skin directory has priority over portable directory, and both have priority over built-in skin with the same id.
- Old `/pet` and `/audio` qrc aliases still exist for compatibility, but built-in Miles manifest uses `skin:`.
- Skin switching resets visible prop/playback state enough to avoid cross-skin residue.
- Custom Interaction registry is reset and re-registered after skin reload.
- Context menu uses native `QMenu`; no custom-painted skin menu.
- Documentation terminology matches code: `SkinDescriptor`, `skin.json`, `manifest.json`, `skin:`, "用户皮肤目录", "便携皮肤目录", "内置皮肤".
