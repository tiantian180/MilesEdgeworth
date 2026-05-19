# Event Action Request Spine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 v2 桌宠“事件 -> 行为管线 -> ActionRequest -> PetRuntime”的主干，让 QML 不再直接调用 Runtime 行为入口。

**Architecture:** QML 只发 `PetEvent`，`InteractionPipeline` 根据当前 `RuntimeSnapshot`、`SkinManifest`、默认规则和 Custom Interaction 生成 `ActionRequest`。`PetRuntime` 只接收 `ActionRequest` 并执行播放、状态和信号更新。

**Tech Stack:** C++17、Qt 6、QML、CTest、Python 静态架构检查。

---

### Task 1: 架构约束测试

**Files:**
- Create: `tests/check_phase_0_59_event_action_request_spine.py`
- Modify: `CMakeLists.txt`

- [ ] 写 `check_phase_0_59_event_action_request_spine.py`，检查事件、请求、快照、交互桥、交互管线和 Custom Interaction 注册器存在。
- [ ] 检查 `PetWindow.qml` 不再直接调用 `PetRuntime` 行为入口。
- [ ] 检查长期设计文档里的 `interruptHint` 统一为 `immediate / afterCurrent`。
- [ ] 注册到 CTest。
- [ ] 运行 `python3 tests/check_phase_0_59_event_action_request_spine.py`，预期失败。

### Task 2: 数据类型与 Runtime 接口

**Files:**
- Create: `apps/desktop/src/pet/events/PetEvent.h`
- Create: `apps/desktop/src/pet/requests/ActionRequest.h`
- Create: `apps/desktop/src/pet/runtime/RuntimeSnapshot.h`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`

- [ ] 定义 `PetEvent`：事件类型、命令 id、点击坐标、窗口尺寸、随机值、prop id。
- [ ] 定义 `ActionRequest`：请求 kind、目标 action/recipe/pool、`interruptHint`、可选 prop id。
- [ ] 定义 `RuntimeSnapshot`：当前 state/action/recipe/phase/facing、sleep 状态、pointer 状态、语言、当前 prop 分支。
- [ ] 在 `PetRuntime` 中新增 `snapshot()` 和 `submitActionRequest(const ActionRequest &request)`。
- [ ] 保持已有播放行为等价。

### Task 3: 交互层

**Files:**
- Create: `apps/desktop/src/pet/interaction/CustomInteractionRegistry.h`
- Create: `apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp`
- Create: `apps/desktop/src/pet/interaction/InteractionPipeline.h`
- Create: `apps/desktop/src/pet/interaction/InteractionPipeline.cpp`
- Create: `apps/desktop/src/pet/events/PetEventBridge.h`
- Create: `apps/desktop/src/pet/events/PetEventBridge.cpp`
- Modify: `apps/desktop/src/main.cpp`
- Modify: `apps/desktop/CMakeLists.txt`

- [ ] `CustomInteractionRegistry` 先处理 `menu.command:miles.feedTea`，返回 `ActionRequest::pool("menu.tea")` 并标记停止默认菜单行为。
- [ ] `InteractionPipeline` 处理单击、双击、睡眠菜单、idle loop、prop clicked/expired。
- [ ] `PetEventBridge` 暴露给 QML，负责把 QML 输入组装成 `PetEvent`，再提交给 pipeline 和 runtime。
- [ ] 在 `main.cpp` 创建 `PetEventBridge` 并注册 QML singleton。

### Task 4: QML 与旧测试迁移

**Files:**
- Modify: `apps/desktop/qml/PetWindow.qml`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: affected `tests/check_phase_0_*.py`

- [ ] QML 行为入口改为 `App.PetEventBridge.*`。
- [ ] 显示属性、尺寸、静音、语言设置暂时继续读写 `App.PetRuntime`。
- [ ] smoke 测试通过 `submitActionRequest` 或 `PetEventBridge` 覆盖行为。
- [ ] 改写固化旧架构的静态测试，测试新事件/request 主干。

### Task 5: 文档与验证

**Files:**
- Modify: `docs/v2/设计方案/皮肤包播放行为设计.md`
- Modify: `docs/v2/设计方案/桌宠运行时与动画调度设计.md`
- Modify: `docs/v2/设计方案/桌宠运行时职责拆分设计.md`
- Modify: `docs/v2/阶段记录/第0阶段桌面壳验证.md`
- Modify: `README.md`

- [ ] 清理 `replace` interruptHint，统一为 `immediate / afterCurrent`。
- [ ] 明确 QML 只发事件，Runtime 只收 ActionRequest。
- [ ] 记录 Phase 0.59 架构纠偏。
- [ ] 运行完整验证：

```bash
python3 tests/check_phase_0_59_event_action_request_spine.py
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build
ctest --test-dir build --output-on-failure
/opt/homebrew/bin/qmllint -I build/apps/desktop -I /opt/homebrew/share/qt/qml apps/desktop/qml/PetWindow.qml
git diff --check
```

- [ ] 启动冒烟：

```bash
pkill -f MilesEdgeworthDesktop || true
open build/apps/desktop/MilesEdgeworthDesktop.app
sleep 2
pgrep -fl MilesEdgeworthDesktop
pkill -f MilesEdgeworthDesktop || true
```

- [ ] 提交：`refactor: add event action request spine`
