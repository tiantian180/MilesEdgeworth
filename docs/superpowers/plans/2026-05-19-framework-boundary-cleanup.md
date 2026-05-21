# Phase 0.60 Framework Boundary Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收紧 v2 桌宠框架边界，移除 Miles 定制硬编码，拆出 Prop 管理，并为透明区域点击穿透建立第一版跨平台入口。

**Architecture:** 本阶段不实现 JS/TS 高级交互执行环境，只把框架可复用能力和皮肤定制内容拆开。简单皮肤命令走 manifest 配置，Prop 作为通用运行时能力由 `PropController` 管理，`PetRuntime` 只保留主体动画播放与状态同步；异型窗口点击穿透由桌面壳层基于 `QWindow::setMask(QRegion)` 实现第一版静态 alpha mask。

**Tech Stack:** Qt 6 / Qt Quick / QML, C++17, CMake/Ninja, Python static checks, `QWindow::setMask`, `QImageReader`, existing `PetRuntimeSmoke`.

---

## Boundary Decisions

本阶段采用以下硬边界：

```text
Framework 通用能力
  PetEvent / ActionRequest / InteractionPipeline
  SkinCommandResolver
  PropController
  WindowInputMaskController

Skin 配置
  manifest.json: skinCommands / props / recipes / actions / hitZones
  assets/: body / audio / props

Skin 定制逻辑
  后续 JS/TS 或 C++ plugin
  本阶段只保留 CustomInteractionRegistry 空壳，不写 Miles 逻辑
```

`Prop` 的定义：

```text
Prop = 桌宠主体身体以外、由运行时临时生成和管理的可视对象。

例如：检察官徽章、掉落物、飘出图标、临时特效。
PropRuntime 是框架能力；具体 prop 内容、触发时机和后续动作属于皮肤配置或自定义交互。
```

本阶段不做：

- 不实现 JS/TS 脚本执行。
- 不实现完整多 Prop QML overlay。`PropController` 内部模型按多实例设计，但 QML 可以先显示当前 active prop，保证旧版手感不回退。
- 不实现逐帧 GIF mask。第一版 input mask 使用当前动画资源第一帧 alpha；后续再扩展 per-frame mask。

## File Structure

新增：

```text
apps/desktop/src/pet/commands/
  SkinCommand.h
  SkinCommandResolver.h
  SkinCommandResolver.cpp

apps/desktop/src/pet/effects/
  PropController.h
  PropController.cpp
  PropState.h

apps/desktop/src/window/
  WindowInputMaskController.h
  WindowInputMaskController.cpp

tests/
  check_phase_0_60_framework_boundaries.py
  check_phase_0_61_prop_controller_split.py
  check_phase_0_62_window_input_mask.py
```

修改：

```text
apps/desktop/resources/skins/miles-edgeworth/manifest.json
apps/desktop/src/pet/manifest/SkinManifest.h
apps/desktop/src/pet/manifest/SkinManifestLoader.cpp
apps/desktop/src/pet/events/PetEventBridge.h
apps/desktop/src/pet/events/PetEventBridge.cpp
apps/desktop/src/pet/interaction/InteractionPipeline.cpp
apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp
apps/desktop/src/pet/PetRuntime.h
apps/desktop/src/pet/PetRuntime.cpp
apps/desktop/src/DesktopShellController.h
apps/desktop/src/DesktopShellController.cpp
apps/desktop/src/main.cpp
apps/desktop/qml/PetWindow.qml
apps/desktop/CMakeLists.txt
CMakeLists.txt
apps/desktop/tests/pet_runtime_smoke.cpp
docs/v2/设计方案/桌宠运行时职责拆分设计.md
docs/v2/设计方案/皮肤包播放行为设计.md
docs/v2/阶段记录/第0阶段桌面壳验证.md
README.md
```

---

## Task 1: Architecture Guard Tests

**Files:**
- Create: `tests/check_phase_0_60_framework_boundaries.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/check_phase_0_60_framework_boundaries.py`:

```python
#!/usr/bin/env python3
"""Phase 0.60: 框架边界检查。

这个检查防止 Miles 皮肤定制逻辑继续混入通用 C++ 框架层。
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    required_files = [
        "apps/desktop/src/pet/commands/SkinCommand.h",
        "apps/desktop/src/pet/commands/SkinCommandResolver.h",
        "apps/desktop/src/pet/commands/SkinCommandResolver.cpp",
        "apps/desktop/src/pet/effects/PropController.h",
        "apps/desktop/src/pet/effects/PropController.cpp",
        "apps/desktop/src/window/WindowInputMaskController.h",
        "apps/desktop/src/window/WindowInputMaskController.cpp",
    ]
    for path in required_files:
        require((ROOT / path).is_file(), f"缺少框架边界文件：{path}")

    cpp_sources = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / "apps/desktop/src").rglob("*")
        if path.suffix in {".cpp", ".h", ".mm"}
    )
    qml = read("apps/desktop/qml/PetWindow.qml")
    manifest = read("apps/desktop/resources/skins/miles-edgeworth/manifest.json")
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    cmake = read("apps/desktop/CMakeLists.txt")

    for forbidden in [
        "miles.feedTea",
        "kFeedTeaCommandId",
        "prosecutorBadgeWindow",
        "prosecutorBadgeImage",
    ]:
        require(forbidden not in cpp_sources, f"通用 C++ 框架层不应硬编码 Miles 定制内容：{forbidden}")
        require(forbidden not in qml, f"通用 QML 壳层不应硬编码 Miles 定制内容：{forbidden}")

    require('"skinCommands"' in manifest, "Miles manifest 应声明 skinCommands")
    require('"miles.feedTea"' in manifest, "Miles 红茶命令应只存在于皮肤 manifest")
    require("currentPropStartOffsetX" not in runtime_h, "Prop 展示字段不应继续挂在 PetRuntime.h")
    require("spawnPropForRecipe" not in runtime_h, "Prop 生命周期不应继续由 PetRuntime 声明")
    require("schedulePropForRecipe" not in runtime_cpp, "Prop 调度不应继续留在 PetRuntime.cpp")
    require("SkinCommandResolver" in cmake, "桌面 CMake 应链接 SkinCommandResolver")
    require("PropController" in cmake, "桌面 CMake 应链接 PropController")
    require("WindowInputMaskController" in cmake, "桌面 CMake 应链接 WindowInputMaskController")
    require(len(runtime_cpp.splitlines()) < 850, "PetRuntime.cpp 应通过本阶段拆分降到 850 行以下")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Register the test**

In root `CMakeLists.txt`, add after Phase 0.59:

```cmake
        add_test(
            NAME check_phase_0_60_framework_boundaries
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_60_framework_boundaries.py
        )
```

- [ ] **Step 3: Run test to verify it fails**

Run:

```bash
python3 tests/check_phase_0_60_framework_boundaries.py
```

Expected: FAIL because `SkinCommandResolver`, `PropController`, and `WindowInputMaskController` do not exist yet, and `miles.feedTea` is still hardcoded in C++/QML.

- [ ] **Step 4: Commit only if this task is executed standalone**

```bash
git add CMakeLists.txt tests/check_phase_0_60_framework_boundaries.py
git commit -m "test: add framework boundary guard"
```

---

## Task 2: Manifest-Driven Skin Commands

**Files:**
- Create: `apps/desktop/src/pet/commands/SkinCommand.h`
- Create: `apps/desktop/src/pet/commands/SkinCommandResolver.h`
- Create: `apps/desktop/src/pet/commands/SkinCommandResolver.cpp`
- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/src/pet/events/PetEventBridge.h`
- Modify: `apps/desktop/src/pet/events/PetEventBridge.cpp`
- Modify: `apps/desktop/src/pet/interaction/InteractionPipeline.cpp`
- Modify: `apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/CMakeLists.txt`
- Test: `tests/check_phase_0_60_framework_boundaries.py`
- Test: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Add skin command model**

Create `apps/desktop/src/pet/commands/SkinCommand.h`:

```cpp
#pragma once

#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <QList>
#include <QString>
#include <QVariantMap>

struct SkinCommandDefinition
{
    QString id;
    QString label;
    QString disabledWhenActionId;
    ActionRequest request;
};

struct ResolvedSkinCommand
{
    QString id;
    QString label;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
        };
    }
};
```

Update `apps/desktop/src/pet/manifest/SkinManifest.h`:

```cpp
#include "pet/commands/SkinCommand.h"
```

Add to `SkinManifest`:

```cpp
QHash<QString, SkinCommandDefinition> skinCommands;
```

- [ ] **Step 2: Parse manifest skinCommands**

In `SkinManifestLoader.cpp`, add helper:

```cpp
ActionRequest requestFromJsonObject(const QJsonObject &object)
{
    const QString type = object.value("type").toString();
    if (type == "pool") {
        return ActionRequest::actionPool(object.value("pool").toString());
    }
    if (type == "recipe") {
        return ActionRequest::recipe(object.value("recipe").toString());
    }
    if (type == "action") {
        return ActionRequest::action(object.value("action").toString());
    }
    if (type == "returnToIdle") {
        return ActionRequest::returnToIdle();
    }
    if (type == "toggleFacing") {
        return ActionRequest::toggleFacing();
    }
    return ActionRequest::none();
}
```

After parsing `clickBehaviors`, add:

```cpp
const QJsonObject skinCommands = root.value("skinCommands").toObject();
for (auto it = skinCommands.constBegin(); it != skinCommands.constEnd(); ++it) {
    const QJsonObject commandObject = it.value().toObject();

    SkinCommandDefinition command;
    command.id = it.key();
    command.label = commandObject.value("label").toString(it.key());
    command.disabledWhenActionId = commandObject
        .value("enabledWhen").toObject()
        .value("notAction").toString();
    command.request = requestFromJsonObject(commandObject.value("request").toObject());

    if (!command.id.isEmpty() && command.request.kind != ActionRequestKind::None) {
        manifest.skinCommands.insert(command.id, command);
    }
}
```

- [ ] **Step 3: Add Miles skin command config**

In `manifest.json`, add near top-level `clickBehaviors`:

```json
"skinCommands": {
  "miles.feedTea": {
    "label": "喂食红茶",
    "enabledWhen": {
      "notAction": "sleep"
    },
    "request": {
      "type": "pool",
      "pool": "menu.tea"
    }
  }
},
```

- [ ] **Step 4: Implement resolver**

Create `apps/desktop/src/pet/commands/SkinCommandResolver.h`:

```cpp
#pragma once

#include "pet/commands/SkinCommand.h"
#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QString>

class SkinCommandResolver
{
public:
    static QList<ResolvedSkinCommand> enabledCommands(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot
    );

    static ActionRequest resolveCommand(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const QString &commandId
    );
};
```

Create `apps/desktop/src/pet/commands/SkinCommandResolver.cpp`:

```cpp
#include "pet/commands/SkinCommandResolver.h"

namespace {
bool commandEnabled(const SkinCommandDefinition &command, const RuntimeSnapshot &snapshot)
{
    if (!command.disabledWhenActionId.isEmpty()
            && snapshot.currentActionId == command.disabledWhenActionId) {
        return false;
    }

    return true;
}
} // namespace

QList<ResolvedSkinCommand> SkinCommandResolver::enabledCommands(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot
)
{
    QList<ResolvedSkinCommand> commands;
    for (const SkinCommandDefinition &command : manifest.skinCommands) {
        if (commandEnabled(command, snapshot)) {
            commands.append({command.id, command.label});
        }
    }
    return commands;
}

ActionRequest SkinCommandResolver::resolveCommand(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const QString &commandId
)
{
    const QString normalizedCommandId = commandId.trimmed();
    if (!manifest.skinCommands.contains(normalizedCommandId)) {
        return ActionRequest::none();
    }

    const SkinCommandDefinition command = manifest.skinCommands.value(normalizedCommandId);
    if (!commandEnabled(command, snapshot)) {
        return ActionRequest::none();
    }

    return command.request;
}
```

- [ ] **Step 5: Use resolver from event bridge and pipeline**

In `PetEventBridge.h`, replace `QStringList enabledSkinCommandIds` with:

```cpp
Q_PROPERTY(QVariantList enabledSkinCommands READ enabledSkinCommands NOTIFY skinCommandAvailabilityChanged)
```

Declare:

```cpp
QVariantList enabledSkinCommands() const;
```

In `PetEventBridge.cpp`, include resolver:

```cpp
#include "pet/commands/SkinCommandResolver.h"
```

Replace the hardcoded command id and method body:

```cpp
QVariantList PetEventBridge::enabledSkinCommands() const
{
    QVariantList commands;
    if (m_runtime == nullptr) {
        return commands;
    }

    const RuntimeSnapshot snapshot = m_runtime->snapshot();
    const QList<ResolvedSkinCommand> resolvedCommands =
        SkinCommandResolver::enabledCommands(m_runtime->manifest(), snapshot);

    for (const ResolvedSkinCommand &command : resolvedCommands) {
        commands.append(command.toVariantMap());
    }
    return commands;
}
```

In `InteractionPipeline.cpp`, include resolver and handle menu command after sleep/return/facing:

```cpp
#include "pet/commands/SkinCommandResolver.h"
```

Inside `MenuCommand` case after built-in commands:

```cpp
else {
    appendIfPlayable(requests, SkinCommandResolver::resolveCommand(manifest, snapshot, event.commandId));
}
```

In `CustomInteractionRegistry.cpp`, remove all `miles.feedTea` logic and return default result:

```cpp
CustomInteractionResult CustomInteractionRegistry::handleEvent(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    Q_UNUSED(manifest);
    Q_UNUSED(snapshot);
    Q_UNUSED(event);

    return {};
}
```

- [ ] **Step 6: Make QML menu generic**

Replace the hardcoded red tea `Platform.MenuItem` with an `Instantiator` inside `contextMenu`:

```qml
Instantiator {
    id: skinCommandItems

    model: App.PetEventBridge.enabledSkinCommands

    delegate: Platform.MenuItem {
        text: modelData.label
        onTriggered: App.PetEventBridge.submitMenuCommand(modelData.id)
    }

    onObjectAdded: function(index, object) {
        contextMenu.insertItem(skinCommandSeparator, object)
    }

    onObjectRemoved: function(index, object) {
        contextMenu.removeItem(object)
    }
}

Platform.MenuSeparator {
    id: skinCommandSeparator
}
```

If `Platform.Menu.insertItem()` is not available in the current Qt version, fallback to a single temporary generic menu item:

```qml
Platform.MenuItem {
    text: App.PetEventBridge.enabledSkinCommands.length > 0
          ? App.PetEventBridge.enabledSkinCommands[0].label
          : ""
    visible: App.PetEventBridge.enabledSkinCommands.length > 0
    enabled: App.PetEventBridge.enabledSkinCommands.length > 0
    onTriggered: App.PetEventBridge.submitMenuCommand(App.PetEventBridge.enabledSkinCommands[0].id)
}
```

Do not keep the string `miles.feedTea` in QML.

- [ ] **Step 7: Update smoke test**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, replace:

```cpp
require(bridge.enabledSkinCommandIds().contains("miles.feedTea"), "待机状态应允许 Miles 红茶皮肤命令");
bridge.submitMenuCommand("miles.feedTea");
```

with helper:

```cpp
auto hasSkinCommand = [](const QVariantList &commands, const QString &commandId) {
    for (const QVariant &value : commands) {
        if (value.toMap().value("id").toString() == commandId) {
            return true;
        }
    }
    return false;
};

require(hasSkinCommand(bridge.enabledSkinCommands(), "miles.feedTea"), "待机状态应允许 Miles 红茶皮肤命令");
bridge.submitMenuCommand("miles.feedTea");
```

The test may still mention `miles.feedTea` because it validates the Miles skin manifest. The static boundary test only forbids this string in framework C++ and QML.

- [ ] **Step 8: Update CMake**

Add to `DESKTOP_SOURCES` and `PetRuntimeSmoke` source list:

```cmake
src/pet/commands/SkinCommand.h
src/pet/commands/SkinCommandResolver.cpp
src/pet/commands/SkinCommandResolver.h
```

- [ ] **Step 9: Verify**

Run:

```bash
python3 tests/check_phase_0_60_framework_boundaries.py
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `check_phase_0_60_framework_boundaries` may still fail on Prop/window mask until later tasks; command-specific tests and build should pass.

- [ ] **Step 10: Commit**

```bash
git add apps/desktop/resources/skins/miles-edgeworth/manifest.json \
        apps/desktop/src/pet/commands \
        apps/desktop/src/pet/manifest/SkinManifest.h \
        apps/desktop/src/pet/manifest/SkinManifestLoader.cpp \
        apps/desktop/src/pet/events/PetEventBridge.h \
        apps/desktop/src/pet/events/PetEventBridge.cpp \
        apps/desktop/src/pet/interaction/InteractionPipeline.cpp \
        apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp \
        apps/desktop/qml/PetWindow.qml \
        apps/desktop/tests/pet_runtime_smoke.cpp \
        apps/desktop/CMakeLists.txt
git commit -m "refactor: make skin commands manifest driven"
```

---

## Task 3: Extract PropController

**Files:**
- Create: `apps/desktop/src/pet/effects/PropState.h`
- Create: `apps/desktop/src/pet/effects/PropController.h`
- Create: `apps/desktop/src/pet/effects/PropController.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/runtime/RuntimeSnapshot.h`
- Modify: `apps/desktop/src/pet/interaction/InteractionPipeline.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Test: `tests/check_phase_0_61_prop_controller_split.py`
- Test: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Write failing Prop split test**

Create `tests/check_phase_0_61_prop_controller_split.py`:

```python
#!/usr/bin/env python3
"""Phase 0.61: PropController 职责拆分检查。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    for path in [
        "apps/desktop/src/pet/effects/PropState.h",
        "apps/desktop/src/pet/effects/PropController.h",
        "apps/desktop/src/pet/effects/PropController.cpp",
    ]:
        require((ROOT / path).is_file(), f"缺少 PropController 文件：{path}")

    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    prop_controller_h = read("apps/desktop/src/pet/effects/PropController.h")
    prop_controller_cpp = read("apps/desktop/src/pet/effects/PropController.cpp")
    qml = read("apps/desktop/qml/PetWindow.qml")
    cmake = read("apps/desktop/CMakeLists.txt")

    for forbidden in [
        "m_currentPropStartOffset",
        "m_currentPropEndOffset",
        "m_currentPropClickedRecipeId",
        "m_currentPropExpiredRecipeId",
        "spawnPropForRecipe",
        "schedulePropForRecipe",
        "propTravelDelta",
    ]:
        require(forbidden not in runtime_h, f"PetRuntime.h 不应持有 Prop 细节：{forbidden}")
        require(forbidden not in runtime_cpp, f"PetRuntime.cpp 不应持有 Prop 细节：{forbidden}")

    for token in [
        "class PropController",
        "scheduleForRecipe",
        "spawn",
        "hide",
        "snapshot",
        "currentPropChanged",
        "currentPropPlaybackSerialChanged",
    ]:
        require(token in prop_controller_h + prop_controller_cpp, f"PropController 缺少能力：{token}")

    require("PropController m_propController" in runtime_h, "PetRuntime 应组合 PropController")
    require("PropOverlay" in qml or "currentProp" in qml, "QML 应通过通用 Prop 表示显示附件")
    require("PropController" in cmake, "CMake 应链接 PropController")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

Register in root `CMakeLists.txt`:

```cmake
        add_test(
            NAME check_phase_0_61_prop_controller_split
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_61_prop_controller_split.py
        )
```

- [ ] **Step 2: Add Prop state model**

Create `apps/desktop/src/pet/effects/PropState.h`:

```cpp
#pragma once

#include <QPointF>
#include <QString>
#include <QUrl>

struct PropState
{
    QString id;
    QUrl imageUrl;
    QPointF startOffset;
    QPointF endOffset;
    double width = 0;
    double height = 0;
    double visualWidth = 0;
    double visualHeight = 0;
    int durationMs = 0;
    QString clickedRecipeId;
    QString expiredRecipeId;
    bool visible = false;
};
```

- [ ] **Step 3: Add PropController interface**

Create `apps/desktop/src/pet/effects/PropController.h`:

```cpp
#pragma once

#include "pet/effects/PropState.h"
#include "pet/manifest/SkinManifest.h"

#include <QObject>

class PropController : public QObject
{
    Q_OBJECT

public:
    explicit PropController(QObject *parent = nullptr);

    const PropState &current() const;
    int playbackSerial() const;

    void scheduleForRecipe(
        const SkinManifest &manifest,
        const RecipeDefinition &recipe,
        const QString &facing,
        double petScale
    );
    void hide();

signals:
    void currentPropChanged();
    void currentPropPlaybackSerialChanged();

private:
    void spawn(const PropDefinition &prop, const QString &facing, double petScale);
    double scaledLength(double length, double petScale) const;
    QPointF pointForFacing(const QHash<QString, QPointF> &points, const QString &facing) const;
    QPointF scaledPoint(const QPointF &point, double petScale) const;
    QPointF travelDelta(const PropDefinition &prop, const QString &facing, double petScale) const;

    PropState m_current;
    int m_requestSerial = 0;
    int m_playbackSerial = 0;
};
```

- [ ] **Step 4: Move Prop implementation**

Create `apps/desktop/src/pet/effects/PropController.cpp` by moving the current logic from `PetRuntime`:

```cpp
#include "pet/effects/PropController.h"

#include <QTimer>
#include <QtGlobal>

PropController::PropController(QObject *parent)
    : QObject(parent)
{
}

const PropState &PropController::current() const
{
    return m_current;
}

int PropController::playbackSerial() const
{
    return m_playbackSerial;
}

void PropController::scheduleForRecipe(
    const SkinManifest &manifest,
    const RecipeDefinition &recipe,
    const QString &facing,
    double petScale
)
{
    if (recipe.propId.isEmpty() || !manifest.props.contains(recipe.propId)) {
        ++m_requestSerial;
        return;
    }

    const PropDefinition prop = manifest.props.value(recipe.propId);
    const int requestSerial = ++m_requestSerial;

    QTimer::singleShot(qMax(0, prop.delayMs), this, [this, prop, facing, petScale, requestSerial]() {
        if (requestSerial != m_requestSerial) {
            return;
        }

        spawn(prop, facing, petScale);
    });
}

void PropController::hide()
{
    ++m_requestSerial;

    if (!m_current.visible && m_current.id.isEmpty()) {
        return;
    }

    m_current = {};
    emit currentPropChanged();
}

void PropController::spawn(const PropDefinition &prop, const QString &facing, double petScale)
{
    const QPointF startOffset = scaledPoint(pointForFacing(prop.startOffsets, facing), petScale);
    const QPointF delta = travelDelta(prop, facing, petScale);

    m_current.visible = true;
    m_current.id = prop.id;
    m_current.imageUrl = prop.assetUrl;
    m_current.startOffset = startOffset;
    m_current.endOffset = startOffset + delta;
    m_current.width = scaledLength(prop.width > 0 ? prop.width : 94, petScale);
    m_current.height = scaledLength(prop.height > 0 ? prop.height : 94, petScale);
    m_current.visualWidth = scaledLength(prop.visualWidth > 0 ? prop.visualWidth : (prop.width > 0 ? prop.width : 94), petScale);
    m_current.visualHeight = scaledLength(prop.visualHeight > 0 ? prop.visualHeight : (prop.height > 0 ? prop.height : 94), petScale);
    m_current.durationMs = prop.durationMs > 0 ? prop.durationMs : 1500;
    m_current.clickedRecipeId = prop.clickedRecipeId;
    m_current.expiredRecipeId = prop.expiredRecipeId;
    ++m_playbackSerial;

    emit currentPropChanged();
    emit currentPropPlaybackSerialChanged();
}

double PropController::scaledLength(double length, double petScale) const
{
    const double safeScale = petScale > 0.0 ? petScale : 2.0;
    return length * safeScale / 2.0;
}

QPointF PropController::pointForFacing(const QHash<QString, QPointF> &points, const QString &facing) const
{
    if (points.contains(facing)) {
        return points.value(facing);
    }
    return points.value(QStringLiteral("right"), {});
}

QPointF PropController::scaledPoint(const QPointF &point, double petScale) const
{
    return QPointF(scaledLength(point.x(), petScale), scaledLength(point.y(), petScale));
}

QPointF PropController::travelDelta(const PropDefinition &prop, const QString &facing, double petScale) const
{
    if (!prop.travelBaseDeltas.isEmpty() || !prop.travelPerScaleDeltas.isEmpty()) {
        const QPointF base = pointForFacing(prop.travelBaseDeltas, facing);
        const QPointF perScale = pointForFacing(prop.travelPerScaleDeltas, facing);
        const double safeScale = petScale > 0.0 ? petScale : 2.0;
        return base + perScale * safeScale;
    }

    return scaledPoint(pointForFacing(prop.travelDeltas, facing), petScale);
}
```

- [ ] **Step 5: Delegate PetRuntime Prop properties**

In `PetRuntime.h`, include:

```cpp
#include "pet/effects/PropController.h"
```

Add member:

```cpp
PropController m_propController;
```

Keep existing QML-facing `currentProp...` properties for compatibility, but implement getters by delegating:

```cpp
bool PetRuntime::currentPropVisible() const
{
    return m_propController.current().visible;
}

QString PetRuntime::currentPropId() const
{
    return m_propController.current().id;
}

QUrl PetRuntime::currentPropImageUrl() const
{
    return m_propController.current().imageUrl;
}
```

Repeat for offsets, dimensions, duration and serial.

In constructor, connect:

```cpp
connect(&m_propController, &PropController::currentPropChanged, this, &PetRuntime::currentPropChanged);
connect(&m_propController, &PropController::currentPropPlaybackSerialChanged, this, &PetRuntime::currentPropPlaybackSerialChanged);
```

Replace `schedulePropForRecipe(...)` call with:

```cpp
m_propController.scheduleForRecipe(m_manifest, m_manifest.recipes.value(nextRecipeId), m_currentFacing, m_petScale);
```

Replace `hideCurrentProp()` implementation with:

```cpp
void PetRuntime::hideCurrentProp()
{
    m_propController.hide();
}
```

Remove old private Prop state fields and helper methods from `PetRuntime`.

- [ ] **Step 6: Update RuntimeSnapshot**

In `PetRuntime::snapshot()`:

```cpp
const PropState &prop = m_propController.current();
snapshot.currentPropId = prop.id;
snapshot.currentPropClickedRecipeId = prop.clickedRecipeId;
snapshot.currentPropExpiredRecipeId = prop.expiredRecipeId;
snapshot.currentPropVisible = prop.visible;
```

- [ ] **Step 7: Update CMake**

Add to `DESKTOP_SOURCES` and `PetRuntimeSmoke`:

```cmake
src/pet/effects/PropController.cpp
src/pet/effects/PropController.h
src/pet/effects/PropState.h
```

- [ ] **Step 8: Verify**

Run:

```bash
python3 tests/check_phase_0_61_prop_controller_split.py
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all pass.

- [ ] **Step 9: Commit**

```bash
git add apps/desktop/src/pet/effects \
        apps/desktop/src/pet/PetRuntime.h \
        apps/desktop/src/pet/PetRuntime.cpp \
        apps/desktop/src/pet/runtime/RuntimeSnapshot.h \
        apps/desktop/src/pet/interaction/InteractionPipeline.cpp \
        apps/desktop/CMakeLists.txt \
        CMakeLists.txt \
        tests/check_phase_0_61_prop_controller_split.py
git commit -m "refactor: extract prop controller"
```

---

## Task 4: Generic Prop Overlay Naming

**Files:**
- Modify: `apps/desktop/qml/PetWindow.qml`
- Test: `tests/check_phase_0_60_framework_boundaries.py`

- [ ] **Step 1: Rename QML prop window identifiers**

In `PetWindow.qml`, rename:

```qml
prosecutorBadgeWindow -> propWindow
prosecutorBadgeImage -> propImage
badgeFlyAnimation -> propFlyAnimation
badgeExpireTimer -> propExpireTimer
```

Keep the data source as `App.PetRuntime.currentProp...` for this phase, because the controller is now behind Runtime. Do not use `prosecutor` or `badge` in QML identifiers.

- [ ] **Step 2: Update comments**

Replace Miles-specific comment:

```qml
// Phase 0.14 的最小 Prop 窗口：先专门承载检察官徽章。
// 它是独立 Window，才能像旧版一样飞出桌宠本体窗口范围。
```

with:

```qml
// 通用 Prop 窗口：用于显示桌宠身体以外的临时对象。
// Miles 的检察官徽章只是当前皮肤定义的一个 prop。
```

- [ ] **Step 3: Verify**

Run:

```bash
python3 tests/check_phase_0_60_framework_boundaries.py
/opt/homebrew/bin/qmllint -I build/apps/desktop -I /opt/homebrew/share/qt/qml apps/desktop/qml/PetWindow.qml
```

Expected: no `prosecutorBadgeWindow` / `prosecutorBadgeImage` in QML; lint passes.

- [ ] **Step 4: Commit**

```bash
git add apps/desktop/qml/PetWindow.qml
git commit -m "refactor: generalize prop overlay naming"
```

---

## Task 5: Window Input Mask Controller

**Files:**
- Create: `apps/desktop/src/window/WindowInputMaskController.h`
- Create: `apps/desktop/src/window/WindowInputMaskController.cpp`
- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/DesktopShellController.cpp`
- Modify: `apps/desktop/qml/PetWindow.qml`
- Modify: `apps/desktop/CMakeLists.txt`
- Test: `tests/check_phase_0_62_window_input_mask.py`

- [ ] **Step 1: Write failing input mask test**

Create `tests/check_phase_0_62_window_input_mask.py`:

```python
#!/usr/bin/env python3
"""Phase 0.62: 透明区域点击穿透输入 mask 检查。"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    for path in [
        "apps/desktop/src/window/WindowInputMaskController.h",
        "apps/desktop/src/window/WindowInputMaskController.cpp",
    ]:
        require((ROOT / path).is_file(), f"缺少窗口输入 mask 文件：{path}")

    controller_h = read("apps/desktop/src/window/WindowInputMaskController.h")
    controller_cpp = read("apps/desktop/src/window/WindowInputMaskController.cpp")
    shell_h = read("apps/desktop/src/DesktopShellController.h")
    shell_cpp = read("apps/desktop/src/DesktopShellController.cpp")
    qml = read("apps/desktop/qml/PetWindow.qml")
    cmake = read("apps/desktop/CMakeLists.txt")

    for token in [
        "class WindowInputMaskController",
        "QRegion",
        "QImageReader",
        "regionFromImage",
        "applyMask",
    ]:
        require(token in controller_h + controller_cpp, f"WindowInputMaskController 缺少 {token}")

    require("setMask" in controller_cpp, "输入 mask 应通过 QWindow::setMask 应用")
    require("setPetInputMask" in shell_h + shell_cpp, "DesktopShellController 应暴露 setPetInputMask")
    require("clearPetInputMask" in shell_h + shell_cpp, "DesktopShellController 应暴露 clearPetInputMask")
    require("setPetInputMask" in qml, "QML 应在动画或尺寸变化时更新输入 mask")
    require("WindowInputMaskController" in cmake, "CMake 应链接 WindowInputMaskController")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

Register in root `CMakeLists.txt`:

```cmake
        add_test(
            NAME check_phase_0_62_window_input_mask
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_62_window_input_mask.py
        )
```

- [ ] **Step 2: Add controller interface**

Create `apps/desktop/src/window/WindowInputMaskController.h`:

```cpp
#pragma once

#include <QRegion>
#include <QSize>
#include <QUrl>

class QWindow;

class WindowInputMaskController
{
public:
    static void applyMask(
        QWindow *window,
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold = 8
    );

    static void clearMask(QWindow *window);

private:
    static QRegion regionFromImage(
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold
    );
};
```

- [ ] **Step 3: Add controller implementation**

Create `apps/desktop/src/window/WindowInputMaskController.cpp`:

```cpp
#include "window/WindowInputMaskController.h"

#include <QImage>
#include <QImageReader>
#include <QRect>
#include <QWindow>

namespace {
QString imagePathFromUrl(const QUrl &url)
{
    if (url.scheme() == "qrc") {
        return QStringLiteral(":") + url.path();
    }
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return url.toString();
}
} // namespace

void WindowInputMaskController::applyMask(
    QWindow *window,
    const QUrl &animationUrl,
    double imageSize,
    double windowSize,
    int alphaThreshold
)
{
    if (window == nullptr || animationUrl.isEmpty()) {
        return;
    }

    const QRegion region = regionFromImage(animationUrl, imageSize, windowSize, alphaThreshold);
    if (region.isEmpty()) {
        clearMask(window);
        return;
    }

    window->setMask(region);
}

void WindowInputMaskController::clearMask(QWindow *window)
{
    if (window == nullptr) {
        return;
    }

    window->setMask(QRegion());
}

QRegion WindowInputMaskController::regionFromImage(
    const QUrl &animationUrl,
    double imageSize,
    double windowSize,
    int alphaThreshold
)
{
    QImageReader reader(imagePathFromUrl(animationUrl));
    reader.setAutoTransform(true);
    const QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }

    const double safeImageSize = imageSize > 0.0 ? imageSize : windowSize;
    const double safeWindowSize = windowSize > 0.0 ? windowSize : safeImageSize;
    const double scaleX = safeImageSize / image.width();
    const double scaleY = safeImageSize / image.height();
    const double offsetX = (safeWindowSize - safeImageSize) / 2.0;
    const double offsetY = (safeWindowSize - safeImageSize) / 2.0;

    QRegion region;
    for (int y = 0; y < image.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x < image.width(); ++x) {
            const bool opaque = qAlpha(image.pixel(x, y)) > alphaThreshold;
            if (opaque && runStart < 0) {
                runStart = x;
            }
            if ((!opaque || x == image.width() - 1) && runStart >= 0) {
                const int runEnd = opaque && x == image.width() - 1 ? x + 1 : x;
                const QRect rect(
                    qRound(offsetX + runStart * scaleX),
                    qRound(offsetY + y * scaleY),
                    qMax(1, qRound((runEnd - runStart) * scaleX)),
                    qMax(1, qRound(scaleY))
                );
                region += rect;
                runStart = -1;
            }
        }
    }

    return region;
}
```

- [ ] **Step 4: Expose from DesktopShellController**

In `DesktopShellController.h`, add:

```cpp
Q_INVOKABLE void setPetInputMask(const QUrl &animationUrl, double imageSize, double windowSize);
Q_INVOKABLE void clearPetInputMask();
```

Include `<QUrl>`.

In `DesktopShellController.cpp`, include:

```cpp
#include "window/WindowInputMaskController.h"
```

Add:

```cpp
void DesktopShellController::setPetInputMask(const QUrl &animationUrl, double imageSize, double windowSize)
{
    WindowInputMaskController::applyMask(m_petWindow, animationUrl, imageSize, windowSize);
}

void DesktopShellController::clearPetInputMask()
{
    WindowInputMaskController::clearMask(m_petWindow);
}
```

- [ ] **Step 5: Call from QML**

In `PetWindow.qml`, add helper:

```qml
function refreshInputMask() {
    App.DesktopShell.setPetInputMask(
        App.PetRuntime.currentAnimationUrl,
        App.PetRuntime.petImageSize,
        App.PetRuntime.petWindowSize
    )
}
```

Call it:

```qml
Component.onCompleted: refreshInputMask()

Connections {
    target: App.PetRuntime

    function onCurrentAnimationUrlChanged() {
        refreshInputMask()
    }

    function onPetScaleChanged() {
        refreshInputMask()
    }
}
```

If an existing `Connections { target: App.PetRuntime }` block already exists, add these functions to it rather than creating a duplicate block.

- [ ] **Step 6: Update CMake**

Add to `DESKTOP_SOURCES`:

```cmake
src/window/WindowInputMaskController.cpp
src/window/WindowInputMaskController.h
```

- [ ] **Step 7: Verify**

Run:

```bash
python3 tests/check_phase_0_62_window_input_mask.py
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop -I /opt/homebrew/share/qt/qml apps/desktop/qml/PetWindow.qml
```

Expected: build/tests/lint pass.

- [ ] **Step 8: Manual check**

Run:

```bash
pkill -f MilesEdgeworthDesktop || true
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Manual expected behavior:

- Dragging/clicking opaque Miles pixels still works.
- Clicking the far transparent corners of the 240x240 window should pass through to the window behind.
- If macOS ignores part of `QWindow::setMask`, record it in `docs/v2/阶段记录/第0阶段桌面壳验证.md` and keep the static implementation; do not add private AppKit hit-test code in this task.

- [ ] **Step 9: Commit**

```bash
git add apps/desktop/src/window \
        apps/desktop/src/DesktopShellController.h \
        apps/desktop/src/DesktopShellController.cpp \
        apps/desktop/qml/PetWindow.qml \
        apps/desktop/CMakeLists.txt \
        CMakeLists.txt \
        tests/check_phase_0_62_window_input_mask.py
git commit -m "feat: add pet window input mask"
```

---

## Task 6: Documentation Alignment

**Files:**
- Modify: `docs/v2/设计方案/桌宠运行时职责拆分设计.md`
- Modify: `docs/v2/设计方案/皮肤包播放行为设计.md`
- Modify: `docs/v2/阶段记录/第0阶段桌面壳验证.md`
- Modify: `README.md`
- Modify: PR #10 description after push

- [ ] **Step 1: Update runtime split design**

In `桌宠运行时职责拆分设计.md`, replace any wording that says `PetRuntime` owns prop state with:

```text
PropController 是框架通用能力，负责管理桌宠主体以外的临时可视对象。
PetRuntime 只在播放 recipe 时把 prop side effect 转交给 PropController，
并把 QML 需要的展示状态转发出去。后续 PropViewModel 稳定后，
QML 将直接读取 PropRuntime / PropOverlay。
```

Add a boundary table:

```text
框架通用：PropController、SkinCommandResolver、WindowInputMaskController
皮肤配置：skinCommands、props、recipes、hitZones
皮肤定制逻辑：后续 JS/TS Custom Interaction
```

- [ ] **Step 2: Update skin playback design**

In `皮肤包播放行为设计.md`, state:

```text
skinCommands 用于描述简单菜单命令到 ActionRequest 的映射。
Custom Interaction 用于描述带概率、条件、Prop 编排、工具调用等复杂逻辑。
本阶段不实现脚本执行环境，先保证 C++ 框架不硬编码具体皮肤命令。
```

- [ ] **Step 3: Update Phase 0 record**

In `第0阶段桌面壳验证.md`, add:

```text
Phase 0.60 收紧框架边界：
- 简单皮肤命令迁入 manifest skinCommands。
- Prop 生命周期迁入 PropController。
- 透明区域点击穿透开始使用 QWindow::setMask 进行公开 API 验证。
```

- [ ] **Step 4: Update README**

Update current status bullet:

```text
- v2 框架边界继续收紧：简单皮肤命令由 manifest 驱动，Prop 生命周期已从 PetRuntime 拆出，桌面壳层开始支持基于 alpha mask 的透明区域点击穿透。
```

- [ ] **Step 5: Verify docs**

Run:

```bash
rg -n 'feedTea|prosecutorBadge|currentPropStartOffsetX|triggerSkinCommand|toggleSleep\\(' apps docs/v2 README.md
```

Expected:

- `feedTea` only appears in `manifest.json`, tests that validate Miles skin config, and docs explaining Miles skin config.
- `prosecutorBadge` should not appear in QML/C++ identifiers.
- `currentPropStartOffsetX` should not appear in `PetRuntime.h`.

- [ ] **Step 6: Commit**

```bash
git add docs/v2/设计方案/桌宠运行时职责拆分设计.md \
        docs/v2/设计方案/皮肤包播放行为设计.md \
        docs/v2/阶段记录/第0阶段桌面壳验证.md \
        README.md
git commit -m "docs: clarify framework and skin boundaries"
```

---

## Task 7: Final Verification and Push

**Files:**
- No source changes unless verification reveals a bug.

- [ ] **Step 1: Run full verification**

```bash
python3 -m py_compile tests/check_phase_0_*.py
python3 tests/check_phase_0_60_framework_boundaries.py
python3 tests/check_phase_0_61_prop_controller_split.py
python3 tests/check_phase_0_62_window_input_mask.py
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop -I /opt/homebrew/share/qt/qml apps/desktop/qml/PetWindow.qml
git diff --check
```

Expected:

```text
ctest: 57/57 passed
qmllint: exit 0
git diff --check: exit 0
```

- [ ] **Step 2: Run app smoke**

```bash
pkill -f MilesEdgeworthDesktop || true
open build/apps/desktop/MilesEdgeworthDesktop.app
sleep 2
pgrep -fl MilesEdgeworthDesktop
pkill -f MilesEdgeworthDesktop || true
```

Expected: process is found by `pgrep`, then exits after `pkill`.

- [ ] **Step 3: Push**

```bash
git status --short --branch
git push origin v2-ai-pet
```

Expected: branch pushes successfully.

- [ ] **Step 4: Update PR #10**

Use `gh pr edit 10 --body ...` and add:

```text
Phase 0.60：
- 简单皮肤命令改为 manifest 驱动，C++/QML 框架层不再硬编码 miles.feedTea。
- Prop 生命周期迁入 PropController，PetRuntime 不再持有 prop 轨迹和展示细节。
- 桌面壳层新增基于 QWindow::setMask 的输入 mask 验证入口，用于透明区域点击穿透。
```

- [ ] **Step 5: Final status**

Report:

```text
提交列表
验证命令与结果
已知限制：input mask 第一版使用 GIF 第一帧，后续需要 per-frame mask / mask asset。
```

---

## Self-Review

- Spec coverage: 覆盖了用户指出的三个问题：`PetRuntime` 继续膨胀、Prop 边界不清、透明区域不穿透。
- Placeholder scan: 没有 `TBD`、`TODO` 或未定义接口。JS/TS Custom Interaction 明确列为非目标。
- Type consistency: `SkinCommandDefinition`、`PropController`、`WindowInputMaskController` 在任务中定义后再使用。
- Scope check: 本计划仍偏大，但每个任务可独立提交。若执行过程中风险过高，优先完成 Task 1-4，窗口 input mask 可单独拆到下一 PR。
