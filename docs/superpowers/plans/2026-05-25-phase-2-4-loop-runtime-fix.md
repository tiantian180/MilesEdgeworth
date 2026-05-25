# Phase 2.4 Loop Runtime Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复聊天动画中 thinking 重复 enter/exit、talking loop 提前退出并冻结的问题，让 enter/loop/exit 的运行时行为符合 Phase 2.4 设计。

**Architecture:** 预切片 GIF 已经解决渲染路径问题，本计划只修运行时调度。`PetRuntime` 保持 cleanFinish 语义为“到安全点后播 exit，再回调”，并补充 recipe 幂等和 phased `returnToIdle()` 收尾；`ChatController` 改为等文字段 drain / pacer empty 后才请求 cleanFinish，避免 talking 在文字吐完前离开 loop。

**Tech Stack:** Qt 6 / C++17 / QWidget / QMovie / QTimer, CMake / CTest, Python contract checks.

---

## Scope Check

本计划只处理 Phase 2.4 loop 调度 bug，不改 Phase 2.4.2 预切片 schema、GIF 生成脚本、Go provider 事件协议和 manifest clip 定义。

不在本计划内：

- 不重新设计 `requestCleanFinishAndNotify()` 的 callback postcondition。
- 不把 direct action 改造成 recipe。
- 不修改 Go sidecar 的 lifecycle / expression 事件格式。
- 不引入新的 manifest 字段。

## File Structure

### Runtime

- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Responsibility: recipe 重复请求幂等、phased action 的 `returnToIdle()` exit 后回 idle。

### Chat Controller

- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Responsibility: GATED / WAITING_FOR_ANIMATION_END 中 cleanFinish 的发起时机，避免文字未吐完时提前 exit。

### Tests

- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`
- Modify: `tests/check_phase_2_4_phased_animation.py`
- Responsibility: 覆盖 thinking recipe 幂等、talking 在慢 pacer 下保持 loop、pacer empty 后 cleanFinish + returnToIdle。

### Docs

- Modify: `docs/v2/设计方案/AI 聊天动画编排设计.md`
- Responsibility: 把“cleanFinish 请求时机”明确为文字条件满足后再请求，避免文档继续暗示 RUN_FINISHED 立刻 cleanFinish。

## Task 1: 移除 ChatController 的 pre-run thinking 重启源

**Files:**

- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write the failing smoke for sendMessage not starting thinking before RUN_STARTED**

In `apps/desktop/tests/chat_controller_smoke.cpp`, after the existing block that checks `sendMessage should invalidate stale pacer chunks before sidecar RUN_STARTED`, add:

```cpp
    // --- Phase 2.4: sendMessage should wait for lifecycle thinking instead of pre-starting thinking ---
    {
        PetRuntime preRunRuntime;
        for (int i = 0; i < 5 && preRunRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            preRunRuntime.handleAnimationFinished();
        }

        ChatController preRunController(&preRunRuntime, &settings);
        preRunController.sendMessage(QStringLiteral("need lifecycle thinking"));

        require(preRunRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "sendMessage must not start thinking before RUN_STARTED/lifecycle events");

        ChatStreamEvent preRunStarted;
        preRunStarted.type = QStringLiteral("RUN_STARTED");
        preRunController.applyStreamEvent(preRunStarted);

        ChatStreamEvent preRunThinking;
        preRunThinking.type = QStringLiteral("CUSTOM");
        preRunThinking.name = QStringLiteral("miles.pet.lifecycle");
        preRunThinking.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        preRunController.applyStreamEvent(preRunThinking);

        for (int i = 0; i < 5 && preRunRuntime.currentState() != QStringLiteral("thinking"); ++i) {
            preRunRuntime.handleAnimationFinished();
        }
        require(preRunRuntime.currentState() == QStringLiteral("thinking"),
                "lifecycle thinking should still start thinking after RUN_STARTED");
    }
```

- [ ] **Step 2: Run the smoke and verify it fails**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build --output-on-failure -R chat_controller_smoke
```

Expected: FAIL at `sendMessage must not start thinking before RUN_STARTED/lifecycle events`.

- [ ] **Step 3: Remove the eager thinking request**

In `apps/desktop/src/chat/ChatController.cpp`, inside `ChatController::sendMessageInConversation`, remove this line:

```cpp
    requestPetExpression(QStringLiteral("thinking"), QStringLiteral("neutral"));
```

Keep `setSending(true)` and `setStatusText(QStringLiteral("正在回复"));`.

- [ ] **Step 4: Run the smoke and verify it passes**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build --output-on-failure -R chat_controller_smoke
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "fix: 避免聊天开始前重复触发 thinking"
```

## Task 2: PetRuntime 同一 runtime-controlled recipe 重复请求幂等

**Files:**

- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Write the failing runtime smoke**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, inside the Phase 2.4 cleanFinish block, immediately after this assertion:

```cpp
        require(runtime.currentRecipeId() == "thinking.holdUntilCancelled",
                "runtime-controlled thinking recipe should remain active while loop is held by runtime");
```

insert:

```cpp
        const int thinkingLoopSerial = runtime.playbackSerial();
        runtime.playRecipe("thinking.holdUntilCancelled");
        require(runtime.currentRecipeId() == "thinking.holdUntilCancelled",
                "duplicate thinking recipe request should keep the active recipe");
        require(runtime.currentPhaseId() == "loop",
                "duplicate thinking recipe request must not restart the enter phase");
        require(runtime.currentRecipeStepRuntimeControlled(),
                "duplicate thinking recipe request should preserve the runtime-controlled loop step");
        require(runtime.playbackSerial() == thinkingLoopSerial,
                "duplicate thinking recipe request must not restart QMovie playback");
```

- [ ] **Step 2: Run the smoke and verify it fails**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: FAIL because duplicate `playRecipe("thinking.holdUntilCancelled")` resets to enter and increments `playbackSerial()`.

- [ ] **Step 3: Add the idempotency guard**

In `apps/desktop/src/pet/PetRuntime.cpp`, inside `PetRuntime::playRecipe`, after validating `m_manifest.recipes.contains(nextRecipeId)` and before assigning `m_currentRecipeId`, add:

```cpp
    if (m_currentRecipeId == nextRecipeId && m_currentRecipeStepRuntimeControlled) {
        return;
    }
```

Do not ignore every duplicate active recipe. The guard is intentionally limited to runtime-controlled loop steps so a future explicit same-recipe request can still restart a one-shot recipe after it has naturally completed or while it is not runtime-held.

- [ ] **Step 4: Run the smoke and verify it passes**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "fix: 保持 runtime recipe 重复请求幂等"
```

## Task 3: ChatController 等文字条件满足后再请求 cleanFinish

**Files:**

- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write the failing WAITING_FOR_ANIMATION_END smoke**

In `apps/desktop/tests/chat_controller_smoke.cpp`, after the existing block named `RUN_FINISHED waits for pacerEmpty before idle`, add:

```cpp
    // --- Phase 2.4: RUN_FINISHED must not cleanFinish talking before pacerEmpty ---
    {
        SettingsService talkingSettings(settingsDir.filePath(QStringLiteral("talking-settings.json")));
        auto talkingCfg = modelConfig(QStringLiteral("talking"),
                                      QStringLiteral("https://api.example.test/v1"),
                                      QStringLiteral("sk-test"),
                                      QStringLiteral("miles-test-model"));
        require(talkingSettings.setModelConfig(talkingCfg.name, talkingCfg),
                "talking settings should accept config");
        talkingSettings.setActiveModelConfig(talkingCfg.name);
        talkingSettings.setMsPerChar(120);

        PetRuntime talkingRuntime;
        for (int i = 0; i < 5 && talkingRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            talkingRuntime.handleAnimationFinished();
        }
        ChatController talkingController(&talkingRuntime, &talkingSettings);

        ChatStreamEvent started;
        started.type = QStringLiteral("RUN_STARTED");
        talkingController.applyStreamEvent(started);

        ChatStreamEvent expr;
        expr.type = QStringLiteral("CUSTOM");
        expr.name = QStringLiteral("miles.pet.expression.requested");
        expr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        expr.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        talkingController.applyStreamEvent(expr);

        ChatStreamEvent textStart;
        textStart.type = QStringLiteral("TEXT_MESSAGE_START");
        textStart.role = QStringLiteral("assistant");
        talkingController.applyStreamEvent(textStart);

        ChatStreamEvent text;
        text.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        text.delta = QStringLiteral("长长长长长长");
        talkingController.applyStreamEvent(text);

        talkingRuntime.handleAnimationFinished();
        require(talkingRuntime.currentActionId() == QStringLiteral("talking"),
                "neutral speaking should activate talking");
        require(talkingRuntime.currentPhaseId() == QStringLiteral("loop"),
                "talking should reach loop before RUN_FINISHED");

        ChatStreamEvent finished;
        finished.type = QStringLiteral("RUN_FINISHED");
        talkingController.applyStreamEvent(finished);

        talkingRuntime.handleAnimationFinished();
        require(talkingRuntime.currentPhaseId() == QStringLiteral("loop"),
                "RUN_FINISHED must not play talking exit while pacer still has text");

        require(waitFor([&talkingController]() {
                    return talkingController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("长长长长长长");
                }, 2000),
                "talking text should drain after RUN_FINISHED");
        require(talkingRuntime.currentPhaseId() == QStringLiteral("exit"),
                "pacerEmpty should request cleanFinish and move talking to exit");

        talkingRuntime.handleAnimationFinished();
        require(waitFor([&talkingRuntime]() {
                    return talkingRuntime.currentState() == QStringLiteral("idle");
                }, 500),
                "runtime should return to idle after talking exit cleanFinish");
    }
```

- [ ] **Step 2: Write the failing GATED smoke**

In the same file, after the WAITING smoke from Step 1, add:

```cpp
    // --- Phase 2.4: GATED must not cleanFinish current segment before segmentDrained ---
    {
        SettingsService gatedSettings(settingsDir.filePath(QStringLiteral("gated-talking-settings.json")));
        auto gatedCfg = modelConfig(QStringLiteral("gated-talking"),
                                    QStringLiteral("https://api.example.test/v1"),
                                    QStringLiteral("sk-test"),
                                    QStringLiteral("miles-test-model"));
        require(gatedSettings.setModelConfig(gatedCfg.name, gatedCfg),
                "gated talking settings should accept config");
        gatedSettings.setActiveModelConfig(gatedCfg.name);
        gatedSettings.setMsPerChar(120);

        PetRuntime gatedRuntime;
        for (int i = 0; i < 5 && gatedRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            gatedRuntime.handleAnimationFinished();
        }
        ChatController gatedController(&gatedRuntime, &gatedSettings);

        ChatStreamEvent started;
        started.type = QStringLiteral("RUN_STARTED");
        gatedController.applyStreamEvent(started);

        ChatStreamEvent firstExpr;
        firstExpr.type = QStringLiteral("CUSTOM");
        firstExpr.name = QStringLiteral("miles.pet.expression.requested");
        firstExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        firstExpr.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        gatedController.applyStreamEvent(firstExpr);

        ChatStreamEvent firstText;
        firstText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        firstText.delta = QStringLiteral("第一段很长很长");
        gatedController.applyStreamEvent(firstText);

        gatedRuntime.handleAnimationFinished();
        require(gatedRuntime.currentPhaseId() == QStringLiteral("loop"),
                "first neutral speaking segment should reach talking loop");

        ChatStreamEvent secondExpr;
        secondExpr.type = QStringLiteral("CUSTOM");
        secondExpr.name = QStringLiteral("miles.pet.expression.requested");
        secondExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        secondExpr.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        gatedController.applyStreamEvent(secondExpr);

        gatedRuntime.handleAnimationFinished();
        require(gatedRuntime.currentPhaseId() == QStringLiteral("loop"),
                "GATED must not play talking exit before first segment text drains");

        require(waitFor([&gatedController]() {
                    return gatedController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString().contains(QStringLiteral("第一段很长很长"));
                }, 2000),
                "first segment should drain before animation gate opens");
        require(gatedRuntime.currentPhaseId() == QStringLiteral("exit"),
                "segmentDrained should request cleanFinish and move current action to exit");

        gatedRuntime.handleAnimationFinished();
        require(gatedRuntime.currentActionId() == QStringLiteral("bow"),
                "after text drain and cleanFinish, queued polite expression should activate");
    }
```

- [ ] **Step 3: Run the smoke and verify both tests fail**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build --output-on-failure -R chat_controller_smoke
```

Expected: FAIL because current code requests cleanFinish immediately in both `RUN_FINISHED` and `GATED`, causing `currentPhaseId()` to become `exit` before text drains.

- [ ] **Step 4: Add delayed-cleanFinish helper declarations**

In `apps/desktop/src/chat/ChatController.h`, after `void requestCleanFinishForCurrentStream();`, add:

```cpp
    void requestGateCleanFinishIfTextDrained();
    void requestFinishCleanFinishIfPacerEmpty();
```

- [ ] **Step 5: Implement delayed-cleanFinish helpers**

In `apps/desktop/src/chat/ChatController.cpp`, immediately after `ChatController::requestCleanFinishForCurrentStream()`, add:

```cpp
void ChatController::requestGateCleanFinishIfTextDrained()
{
    if (m_phase != ChatPhase::GATED
            || m_animationReady
            || !m_textDrained
            || m_cleanFinishRequestPending
            || m_deferredCleanFinishStreamId != 0) {
        return;
    }

    m_gateTimeout.start(kGateTimeoutMs);
    requestCleanFinishForCurrentStream();
}

void ChatController::requestFinishCleanFinishIfPacerEmpty()
{
    if (m_phase != ChatPhase::WAITING_FOR_ANIMATION_END
            || m_animationReady
            || !m_pacerEmpty
            || m_cleanFinishRequestPending
            || m_deferredCleanFinishStreamId != 0) {
        return;
    }

    requestCleanFinishForCurrentStream();
}
```

- [ ] **Step 6: Replace immediate WAITING cleanFinish requests**

In `ChatController::applyStreamEvent`, replace both `RUN_FINISHED` branches that do:

```cpp
            requestCleanFinishForCurrentStream();
            maybeFinishWaitingForAnimationEnd();
```

with:

```cpp
            requestFinishCleanFinishIfPacerEmpty();
            maybeFinishWaitingForAnimationEnd();
```

In `ChatController::handleCleanFinishReady`, replace both `streamFinished` branches that enter `WAITING_FOR_ANIMATION_END` and call `requestCleanFinishForCurrentStream()` with `requestFinishCleanFinishIfPacerEmpty()`.

In `ChatController::maybeAdvanceGate`, replace the final `streamFinished` branch:

```cpp
        requestCleanFinishForCurrentStream();
        maybeFinishWaitingForAnimationEnd();
```

with:

```cpp
        requestFinishCleanFinishIfPacerEmpty();
        maybeFinishWaitingForAnimationEnd();
```

- [ ] **Step 7: Replace immediate GATED cleanFinish requests**

In `ChatController::enterGateForNextSegment`, replace:

```cpp
    m_gateTimeout.start(kGateTimeoutMs);
    requestCleanFinishForCurrentStream();
    maybeAdvanceGate();
```

with:

```cpp
    requestGateCleanFinishIfTextDrained();
    maybeAdvanceGate();
```

In `ChatController::maybeAdvanceGate`, inside the `if (!m_segmentQueue.isEmpty())` block, replace:

```cpp
        m_gateTimeout.start(kGateTimeoutMs);
        requestCleanFinishForCurrentStream();
        maybeAdvanceGate();
```

with:

```cpp
        requestGateCleanFinishIfTextDrained();
        maybeAdvanceGate();
```

In `ChatController::handleSegmentDrained`, replace:

```cpp
        m_textDrained = true;
        maybeAdvanceGate();
```

with:

```cpp
        m_textDrained = true;
        requestGateCleanFinishIfTextDrained();
        maybeAdvanceGate();
```

In `ChatController::handlePacerEmpty`, before `maybeFinishWaitingForAnimationEnd();`, add:

```cpp
    requestFinishCleanFinishIfPacerEmpty();
```

- [ ] **Step 8: Keep deferred cleanFinish aligned with text gates**

In `ChatController::requestDeferredCleanFinishIfPossible`, replace the phase condition:

```cpp
            && (m_phase == ChatPhase::BUFFERING_FOR_START
                || m_phase == ChatPhase::GATED
                || m_phase == ChatPhase::WAITING_FOR_ANIMATION_END)) {
```

with:

```cpp
            && (m_phase == ChatPhase::BUFFERING_FOR_START
                || (m_phase == ChatPhase::GATED && m_textDrained)
                || (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END && m_pacerEmpty))) {
```

- [ ] **Step 9: Run the smoke and verify it passes**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build --output-on-failure -R chat_controller_smoke
```

Expected: PASS.

- [ ] **Step 10: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "fix: 延后聊天动画 cleanFinish 请求"
```

## Task 4: PetRuntime returnToIdle 播 phased exit 后回 idle

**Files:**

- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Write the failing runtime smoke**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, inside the Phase 2.4 cleanFinish block, after the `suppressed cleanFinish` assertions and before `runtime.setSuppressAutoIdle(false);`, add:

```cpp
        runtime.setSuppressAutoIdle(false);
        runtime.playAction("talking");
        runtime.handleAnimationFinished();
        require(runtime.currentPhaseId() == "loop",
                "talking should reach loop before direct returnToIdle");
        runtime.returnToIdle();
        require(runtime.currentPhaseId() == "exit",
                "direct returnToIdle from phased action should play exit first");
        runtime.handleAnimationFinished();
        require(runtime.currentState() == "idle",
                "direct returnToIdle should switch to idle after phased exit finishes");
        require(runtime.currentActionId() == "idle_stand",
                "direct returnToIdle should restore idle action after phased exit finishes");
```

Remove the later duplicate `runtime.setSuppressAutoIdle(false);` in the same block if this insertion creates two consecutive calls.

- [ ] **Step 2: Run the smoke and verify it fails**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: FAIL because `returnToIdle()` plays `exit` and then leaves the runtime held on the exit frame.

- [ ] **Step 3: Add the pending idle flag**

In `apps/desktop/src/pet/PetRuntime.h`, add this private field near `m_cleanFinishExitInProgress`:

```cpp
    bool m_returnToIdleAfterExit = false;
```

- [ ] **Step 4: Set and clear the flag in returnToIdle and new playback**

In `apps/desktop/src/pet/PetRuntime.cpp`, update `PetRuntime::returnToIdle()`:

```cpp
    clearActiveRecipe();

    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    if (!action.exitPhase.isEmpty()
            && m_currentPhaseId != action.exitPhase
            && action.phases.contains(action.exitPhase)) {
        m_returnToIdleAfterExit = true;
        playPhase(m_currentActionId, action.exitPhase);
        return;
    }

    m_returnToIdleAfterExit = false;
    setState("idle");
    continueCleanFinishIfPossible();
```

In `PetRuntime::playRecipe`, before setting `m_currentRecipeId = nextRecipeId;`, add:

```cpp
    m_returnToIdleAfterExit = false;
```

In `PetRuntime::playActionInternal`, before `setCurrentAction(nextActionId, m_manifest.actions.value(nextActionId));`, add:

```cpp
    m_returnToIdleAfterExit = false;
```

- [ ] **Step 5: Complete pending idle after exit finishes**

In `PetRuntime::handleAnimationFinished()`, immediately after:

```cpp
    if (continueCleanFinishIfPossible()) {
        return;
    }
```

add:

```cpp
    if (m_returnToIdleAfterExit) {
        m_returnToIdleAfterExit = false;
        setState(QStringLiteral("idle"));
        continueCleanFinishIfPossible();
        return;
    }
```

This must stay after `continueCleanFinishIfPossible()` so cleanFinish callbacks keep priority when a cleanFinish request owns the exit transition.

- [ ] **Step 6: Run the smoke and verify it passes**

Run:

```bash
cmake --build build --target PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R pet_runtime_smoke
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "fix: 修复 phased 动画 returnToIdle 收尾"
```

## Task 5: Contract check and design document alignment

**Files:**

- Modify: `tests/check_phase_2_4_phased_animation.py`
- Modify: `docs/v2/设计方案/AI 聊天动画编排设计.md`

- [ ] **Step 1: Extend the static contract check**

In `tests/check_phase_2_4_phased_animation.py`, add checks that fail if the old eager cleanFinish patterns return:

```python
    chat_controller = read("apps/desktop/src/chat/ChatController.cpp")
    require(
        "requestGateCleanFinishIfTextDrained" in chat_controller,
        "ChatController must gate cleanFinish requests on segmentDrained in GATED"
    )
    require(
        "requestFinishCleanFinishIfPacerEmpty" in chat_controller,
        "ChatController must gate final cleanFinish requests on pacerEmpty"
    )
    require(
        "requestPetExpression(QStringLiteral(\"thinking\"), QStringLiteral(\"neutral\"));" not in
        chat_controller[chat_controller.index("void ChatController::sendMessageInConversation"):chat_controller.index("void ChatController::cancelCurrentReply")],
        "sendMessageInConversation must wait for lifecycle thinking instead of pre-starting thinking"
    )
```

If the file already has `chat_controller` text loaded under another variable name, reuse that variable instead of reading the file twice.

- [ ] **Step 2: Run the contract check**

Run:

```bash
python3 tests/check_phase_2_4_phased_animation.py
```

Expected: PASS.

- [ ] **Step 3: Update AI animation orchestration design**

In `docs/v2/设计方案/AI 聊天动画编排设计.md`, update the GATED and WAITING descriptions:

Replace the GATED `expression.requested` row text that says entering GATED immediately requests cleanFinish with:

```markdown
| `expression.requested` | 创建新 segment 入队，转 GATED。进入 GATED 时初始化双条件标志：`m_animationReady = false`；`m_textDrained` 根据当前 pacer 状态初始化。只有 `m_textDrained == true` 后才请求 `requestCleanFinishAndNotify`，避免当前段文字还在吐出时过早播放 exit。 |
```

Replace the WAITING description that says RUN_FINISHED immediately requests cleanFinish with:

```markdown
| `RUN_FINISHED` | 标记 `streamFinished`。queue 应已空（STREAMING 时 queue 是空的）。转 WAITING_FOR_ANIMATION_END。若 pacer 已空，立即请求 `requestCleanFinishAndNotify`；若 pacer 仍在吐 active segment 的文字，先继续保持当前 loop，等 `pacerEmpty` 后再请求 cleanFinish。 |
```

- [ ] **Step 4: Run doc and contract verification**

Run:

```bash
python3 tests/check_phase_2_4_phased_animation.py
git diff --check
```

Expected: both pass.

- [ ] **Step 5: Commit**

```bash
git add tests/check_phase_2_4_phased_animation.py docs/v2/设计方案/AI\ 聊天动画编排设计.md
git commit -m "docs: 明确聊天 cleanFinish 请求时机"
```

## Task 6: Full verification

**Files:**

- No code changes.

- [ ] **Step 1: Reconfigure if CMake files or tests changed**

Run:

```bash
cmake -S . -B build
```

Expected: configure and generate complete without errors.

- [ ] **Step 2: Build**

Run:

```bash
cmake --build build
```

Expected: build completes without errors.

- [ ] **Step 3: Run targeted checks**

Run:

```bash
ctest --test-dir build --output-on-failure -R 'pet_runtime_smoke|chat_controller_smoke|check_phase_2_4_phased_animation'
```

Expected: all targeted tests pass.

- [ ] **Step 4: Run full CTest**

Run:

```bash
ctest --test-dir build --output-on-failure
```

Expected: 100% tests passed.

- [ ] **Step 5: Run diff hygiene**

Run:

```bash
git diff --check
git status --short --ignored=matching apps/desktop/resources/skins/miles-edgeworth/generated
```

Expected: `git diff --check` exits 0; status has no tracked/untracked changes except ignored `generated/`.

- [ ] **Step 6: Request review**

Use a review subagent or manual code review focused on:

```text
1. Does talking remain in loop until pacerEmpty?
2. Does GATED wait for segmentDrained before requesting cleanFinish?
3. Does thinking lifecycle avoid replaying enter while already in runtime-controlled loop?
4. Does returnToIdle from a phased action reach idle after exit?
5. Did any change weaken cleanFinish callback postcondition?
```

Expected: no blocker findings.

## Self-Review

- Spec coverage: Task 1 removes the likely self-inflicted pre-run thinking replay. Task 2 covers duplicate runtime-controlled thinking requests. Task 3 fixes premature cleanFinish in both GATED and WAITING_FOR_ANIMATION_END. Task 4 fixes the `returnToIdle()` exit freeze that would otherwise appear once cleanFinish is delayed. Task 5 keeps long-term design docs and contract checks aligned.
- Placeholder scan: no placeholder markers or unspecified test command remains.
- Type consistency: helper names are consistently `requestGateCleanFinishIfTextDrained` and `requestFinishCleanFinishIfPacerEmpty`; runtime flag name is consistently `m_returnToIdleAfterExit`.
