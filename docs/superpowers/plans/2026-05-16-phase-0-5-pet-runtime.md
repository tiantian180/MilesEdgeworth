# Phase 0.5 Pet Runtime Minimal Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 v2 桌宠从“QML 写死一个 GIF”推进到“由 C++ PetRuntime 根据状态选择动画”，并把桌面壳层对象改成 QML 可识别的 singleton。

**Architecture:** `main.cpp` 继续负责创建长生命周期 C++ 对象；`DesktopShellController` 负责窗口壳层；新增 `PetRuntime` 负责最小动作状态。两个 C++ 对象通过 Qt QML foreign singleton 暴露给 QML，让运行时和 QML Language Server 都能识别。

**Tech Stack:** Qt 6、Qt Quick/QML、C++17、CMake、CTest、Python 静态检查脚本。

---

## File Structure

- Modify: `CMakeLists.txt`
  - 把新的 Phase 0.5 检查脚本接入 CTest。
- Modify: `apps/desktop/CMakeLists.txt`
  - 增加 `PetRuntime` 源文件。
  - 将 `DesktopShellController.h`、`PetRuntime.h` 放入 QML module `SOURCES`，生成 QML 类型信息。
- Modify: `apps/desktop/src/main.cpp`
  - 删除 `setContextProperty("desktopShell", ...)`。
  - 创建 `PetRuntime`。
  - 设置 QML singleton foreign wrapper 的静态实例指针。
- Modify: `apps/desktop/src/DesktopShellController.h`
  - 增加 QML foreign singleton wrapper，QML 名称为 `DesktopShell`。
- Create: `apps/desktop/src/pet/PetRuntime.h`
  - 声明最小动作状态接口和 QML singleton wrapper，QML 名称为 `PetRuntime`。
- Create: `apps/desktop/src/pet/PetRuntime.cpp`
  - 从 qrc manifest 加载 state/action 映射，并暴露测试动作入口。
- Modify: `apps/desktop/qml/PetWindow.qml`
  - 使用 `DesktopShell` 和 `PetRuntime` singleton。
  - 右键菜单增加“回到待机 / 测试思考 / 测试说话”开发入口。
- Modify: `apps/desktop/resources/pet_assets.qrc`
  - 增加 Phase 0.5 需要的少量动作 GIF 和 manifest。
- Create: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
  - 定义 `idle`、`thinking`、`speaking` 三个状态。
- Create: `tests/check_phase_0_5_pet_runtime.py`
  - 静态检查最小纵切是否接好。
- Modify: `docs/v2/phase0-desktop-shell.md`
  - 追加 Phase 0.5 使用方式和验证命令。

---

### Task 1: Add Phase 0.5 Regression Test

**Files:**
- Create: `tests/check_phase_0_5_pet_runtime.py`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/check_phase_0_5_pet_runtime.py` with checks for the intended API surface:

```python
#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


main_cpp = read("apps/desktop/src/main.cpp")
pet_window_qml = read("apps/desktop/qml/PetWindow.qml")
cmake = read("apps/desktop/CMakeLists.txt")
qrc = read("apps/desktop/resources/pet_assets.qrc")

require('setContextProperty("desktopShell"' not in main_cpp, "desktopShell must not be injected as a context property")
require("PetRuntime petRuntime" in main_cpp, "main.cpp should create the PetRuntime singleton instance")
require("DesktopShellControllerForeign::s_instance = &shellController" in main_cpp, "DesktopShell singleton instance is not wired")
require("PetRuntimeForeign::s_instance = &petRuntime" in main_cpp, "PetRuntime singleton instance is not wired")

require("desktopShell." not in pet_window_qml, "PetWindow.qml should use DesktopShell singleton instead of desktopShell")
require("DesktopShell.alwaysOnTop" in pet_window_qml, "PetWindow.qml should read DesktopShell.alwaysOnTop")
require("DesktopShell.toggleAlwaysOnTop()" in pet_window_qml, "PetWindow.qml should call DesktopShell.toggleAlwaysOnTop()")
require("PetRuntime.currentAnimationUrl" in pet_window_qml, "PetWindow.qml should bind AnimatedImage to PetRuntime.currentAnimationUrl")
require("PetRuntime.testThinking()" in pet_window_qml, "PetWindow.qml should expose a thinking test action")
require("PetRuntime.testSpeaking()" in pet_window_qml, "PetWindow.qml should expose a speaking test action")
require('source: "qrc:/pet/stand-right.gif"' not in pet_window_qml, "PetWindow.qml should not hard-code the idle gif")

require("src/pet/PetRuntime.cpp" in cmake, "PetRuntime.cpp should be part of the desktop target")
require("src/pet/PetRuntime.h" in cmake, "PetRuntime.h should be part of the QML module sources")

for alias in ["thinking-right.gif", "objecting-right.gif", "manifest.json"]:
    require(f'alias="{alias}"' in qrc, f"qrc is missing {alias}")

manifest_path = ROOT / "apps/desktop/resources/skins/miles-edgeworth/manifest.json"
require(manifest_path.exists(), "skin manifest is missing")
manifest = manifest_path.read_text(encoding="utf-8")
for state in ['"idle"', '"thinking"', '"speaking"', '"fallbackAction"']:
    require(state in manifest, f"manifest is missing {state}")
```

- [ ] **Step 2: Register the test in CTest**

In the root `CMakeLists.txt`, add this command next to the existing Python check:

```cmake
add_test(
    NAME check_phase_0_5_pet_runtime
    COMMAND ${Python3_EXECUTABLE} tests/check_phase_0_5_pet_runtime.py
)
```

- [ ] **Step 3: Run the new test and verify it fails**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
ctest --test-dir build -R check_phase_0_5_pet_runtime --output-on-failure
```

Expected: the new test fails because `PetRuntime` and the QML singletons are not implemented yet.

---

### Task 2: Expose DesktopShell as a QML Singleton

**Files:**
- Modify: `apps/desktop/src/DesktopShellController.h`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/qml/PetWindow.qml`

- [ ] **Step 1: Add the foreign singleton wrapper**

Append this wrapper in `DesktopShellController.h` after the class declaration:

```cpp
// 这个 wrapper 只负责告诉 QML 类型系统：
// 已经存在的 DesktopShellController 对象要作为 DesktopShell 单例暴露。
struct DesktopShellControllerForeign
{
    Q_GADGET
    QML_FOREIGN(DesktopShellController)
    QML_NAMED_ELEMENT(DesktopShell)
    QML_SINGLETON

public:
    inline static DesktopShellController *s_instance = nullptr;

    static DesktopShellController *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(engine->thread() == s_instance->thread());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
```

- [ ] **Step 2: Wire the singleton instance in main.cpp**

Replace context-property injection with:

```cpp
DesktopShellController shellController;
DesktopShellControllerForeign::s_instance = &shellController;
```

- [ ] **Step 3: Update QML references**

Replace:

```qml
desktopShell.alwaysOnTop
desktopShell.toggleAlwaysOnTop()
```

with:

```qml
DesktopShell.alwaysOnTop
DesktopShell.toggleAlwaysOnTop()
```

- [ ] **Step 4: Reconfigure and build**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
```

Expected: build still succeeds.

---

### Task 3: Add Minimal PetRuntime and Skin Manifest

**Files:**
- Create: `apps/desktop/src/pet/PetRuntime.h`
- Create: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/CMakeLists.txt`
- Modify: `apps/desktop/resources/pet_assets.qrc`
- Create: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/qml/PetWindow.qml`

- [ ] **Step 1: Add PetRuntime API**

`PetRuntime` exposes:

```cpp
Q_PROPERTY(QString currentState READ currentState NOTIFY currentStateChanged)
Q_PROPERTY(QString currentActionId READ currentActionId NOTIFY currentActionChanged)
Q_PROPERTY(QUrl currentAnimationUrl READ currentAnimationUrl NOTIFY currentAnimationUrlChanged)

Q_INVOKABLE void setState(const QString &state);
Q_INVOKABLE void playAction(const QString &actionId);
Q_INVOKABLE void returnToIdle();
Q_INVOKABLE void testThinking();
Q_INVOKABLE void testSpeaking();
```

- [ ] **Step 2: Load the minimal manifest**

Read `:/pet/manifest.json`, parse `states` and `actions`, and fall back to `qrc:/pet/stand-right.gif` when loading fails.

- [ ] **Step 3: Add QML singleton wrapper**

Expose the existing C++ object as QML singleton named `PetRuntime` using `QML_FOREIGN`, `QML_NAMED_ELEMENT(PetRuntime)`, and `QML_SINGLETON`.

- [ ] **Step 4: Wire PetRuntime in main.cpp**

Create the object before `engine.loadFromModule()`:

```cpp
PetRuntime petRuntime;
PetRuntimeForeign::s_instance = &petRuntime;
```

- [ ] **Step 5: Bind AnimatedImage to PetRuntime**

In `PetWindow.qml`, set:

```qml
source: PetRuntime.currentAnimationUrl
```

and add menu items for:

```qml
PetRuntime.returnToIdle()
PetRuntime.testThinking()
PetRuntime.testSpeaking()
```

- [ ] **Step 6: Run the Phase 0.5 test**

Run:

```bash
ctest --test-dir build -R check_phase_0_5_pet_runtime --output-on-failure
```

Expected: the Phase 0.5 test passes.

---

### Task 4: Verify Build, QML, and Existing Checks

**Files:**
- Modify if needed: `tests/check_macos_window_behavior.py`
- Modify if needed: `docs/v2/phase0-desktop-shell.md`

- [ ] **Step 1: Run full build**

```bash
cmake --build build
```

Expected: build exits with code 0.

- [ ] **Step 2: Run all CTest checks**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all registered tests pass.

- [ ] **Step 3: Run QML lint through the built module**

```bash
/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml
```

Expected: no warnings for `DesktopShell` or `PetRuntime`.

- [ ] **Step 4: Document the manual verification**

Append a short section to `docs/v2/phase0-desktop-shell.md` showing:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop apps/desktop/qml/PetWindow.qml
```

---

### Task 5: Commit, Push, and Update Draft PR

**Files:**
- All files changed above.

- [ ] **Step 1: Review diff**

Run:

```bash
git status --short
git diff --stat
git diff --check
```

Expected: only Phase 0.5 code, test, resource, and documentation files are changed; `git diff --check` exits with code 0.

- [ ] **Step 2: Commit**

Run:

```bash
git add CMakeLists.txt apps/desktop docs tests
git commit -m "feat: add minimal pet runtime"
```

- [ ] **Step 3: Push**

Run:

```bash
git push
```

- [ ] **Step 4: Update PR description**

Update draft PR #10 to mention:

- QML singleton registration replaces context property.
- Minimal `PetRuntime` drives idle/thinking/speaking animations from manifest.
- Phase 0.5 tests and QML lint verification were run.

---

## Self-Review

- Spec coverage: covers QML singleton, PetRuntime, manifest, QML menu test actions, CTest, docs, PR update.
- Placeholder scan: no `TBD` or unresolved implementation placeholders are used as plan instructions.
- Type consistency: QML names are `DesktopShell` and `PetRuntime`; C++ foreign wrappers are `DesktopShellControllerForeign` and `PetRuntimeForeign`; runtime property is `currentAnimationUrl`.
