# Phase 2.4 Phased 动画与动画链 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 Phase 2.4 的 phased 动画、cleanFinish 统一接口、segment queue 双条件门控、SSE lifecycle 拆分和设置保存静默重载。

**Architecture:** Qt 侧把播放控制分成三层：`PetRuntime` 负责动画 clean finish 和 recipe step 推进，`ChatTextPacer` 负责按 stream/segment 吐字，`ChatController` 负责 reply session 状态机和双条件门控。Go sidecar 只负责把 provider token 拆成 lifecycle、expression 和 text 三类事件，不再把 idle 当成文字段事件。Manifest 扩展 clips/frameRange、onceThenHold 和 runtime-controlled recipe step，保持旧 recipe 能继续顺序执行。

**Tech Stack:** Qt 6 / C++17 / QWidget / QMovie / QTimer, Go OpenAI-compatible SSE provider, Python contract checks, CMake / CTest.

---

## Scope Check

Phase 2.4 覆盖多个模块，但这些模块都服务同一个可验收闭环：聊天回复期间动画和文字按 expression 分段同步。计划按可独立验证的提交拆分，每个任务都能运行局部 smoke 或 contract check。

不在本计划内：

- Pet Skin Studio 和动画资源预切分工具。
- Phase 2.5 移动 API。
- Phase 2.6 多模态。
- 大规模抽 `PlaybackController` / `RecipeRunner`。本阶段只在现有 `PetRuntime` 内收口必要状态，避免重构和功能实现混在一起。

## File Structure

### Qt Runtime

- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/PetRuntimeSkin.cpp`
- Responsibility: 当前动画、phase、recipe step、cleanFinish callback、自动 idle 兜底、皮肤 reload 模式。

### Native Surface

- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Responsibility: QMovie 播放、frameRange 边界、onceThenHold 定帧、loop 边界回报。

### QML Legacy Surface

- Modify: `apps/desktop/qml/PetWindow.qml`
- Responsibility: 维持旧 QML surface 的 loopMode 行为一致性。当前主窗口使用 native surface，但 contract checks 仍覆盖 QML 文件。

### Manifest Loader

- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Responsibility: clips/frameRange、AnimationVariant、onceThenHold、recipe step `duration: "runtime"`。

### Chat Pacer

- Modify: `apps/desktop/src/chat/ChatTextPacer.h`
- Modify: `apps/desktop/src/chat/ChatTextPacer.cpp`
- Responsibility: streamId 防串流、segmentId 分段 drain、pacerEmpty 信号。

### Chat Controller

- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Responsibility: lifecycle 事件、segment queue、BUFFERING/GATED/WAITING 双条件门控、reply session 结束。

### Go Sidecar

- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Responsibility: `miles.pet.lifecycle` thinking 事件、expression 文字段事件、移除结束 idle expression。

### Tests And Contract Checks

- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Modify: `apps/desktop/tests/chat_text_pacer_smoke.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`
- Modify: `tests/check_phase_0_6_animation_runtime.py`
- Modify: `tests/check_phase_0_7_phase_runtime.py`
- Create: `tests/check_phase_2_4_phased_animation.py`
- Modify: `CMakeLists.txt`
- Responsibility: 单元 smoke、静态 contract、阶段验收。

### Docs

- Modify: `docs/v2/参考资料/技术债务与评审待办.md`
- Modify: `README.md`
- Responsibility: 清掉已解决的 P2.4 债务入口，README 补 Phase 2.4 文档链接。

## Task 1: 设置保存静默重载

**Files:**

- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntimeSkin.cpp`
- Modify: `apps/desktop/src/settings/SettingsController.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/tests/settings_service_smoke.cpp`

- [ ] **Step 1: Write the failing runtime smoke for preserve reload**

Insert this block in `apps/desktop/tests/pet_runtime_smoke.cpp` immediately after the existing `require(runtime.reloadActiveSkin(), "runtime should reload active skin");` assertion:

```cpp
    runtime.playAction("objecting");
    const int preserveReloadSerial = runtime.playbackSerial();
    require(runtime.reloadActiveSkinPreservingPlayback(),
            "preserve reload should reload the active skin manifest");
    require(runtime.currentActionId() == "objecting",
            "preserve reload must keep the current action instead of replaying startup");
    require(runtime.currentRecipeId().isEmpty(),
            "preserve reload must not start startup.briefcase");
    require(runtime.playbackSerial() == preserveReloadSerial,
            "preserve reload must not restart the current animation");
    runtime.returnToIdle();
```

- [ ] **Step 2: Write the failing settings smoke for persona save**

In `apps/desktop/tests/settings_service_smoke.cpp`, inside the persona save block that creates `SettingsController controller(&service, &runtime);`, insert before `controller.save();`:

```cpp
        runtime.playAction("objecting");
        const int playbackSerialBeforePersonaSave = runtime.playbackSerial();
```

Insert after `assert(controller.personaError().isEmpty());`:

```cpp
        assert(runtime.currentActionId() == QStringLiteral("objecting"));
        assert(runtime.playbackSerial() == playbackSerialBeforePersonaSave);
        assert(runtime.currentRecipeId().isEmpty());
```

- [ ] **Step 3: Run tests to verify they fail**

Run:

```bash
cmake --build build --target PetRuntimeSmoke SettingsServiceSmoke
ctest --test-dir build --output-on-failure -R 'pet_runtime_smoke|settings_service_smoke'
```

Expected: FAIL because `reloadActiveSkinPreservingPlayback` is not declared.

- [ ] **Step 4: Add the public preserve reload API**

In `apps/desktop/src/pet/PetRuntime.h`, add the enum and method declarations:

```cpp
public:
    enum class SkinReloadMode {
        PlayStartup,
        PreservePlayback,
    };

    Q_INVOKABLE bool reloadActiveSkin();
    Q_INVOKABLE bool reloadActiveSkinPreservingPlayback();

private:
    bool reloadActiveSkin(SkinReloadMode mode);
    bool loadSkinDescriptor(const SkinDescriptor &descriptor, SkinReloadMode mode);
```

Replace the old private declaration:

```cpp
    bool loadSkinDescriptor(const SkinDescriptor &descriptor);
```

with the mode-aware declaration above.

- [ ] **Step 5: Implement mode-aware skin activation**

In `apps/desktop/src/pet/PetRuntimeSkin.cpp`, replace `reloadActiveSkin()` and `loadSkinDescriptor(...)` with mode-aware versions. Keep `setActiveSkin()` and `activateSkin()` full reload behavior:

```cpp
bool PetRuntime::reloadActiveSkin()
{
    return reloadActiveSkin(SkinReloadMode::PlayStartup);
}

bool PetRuntime::reloadActiveSkinPreservingPlayback()
{
    return reloadActiveSkin(SkinReloadMode::PreservePlayback);
}

bool PetRuntime::reloadActiveSkin(SkinReloadMode mode)
{
    refreshAvailableSkins();
    return loadSkinDescriptor(descriptorForSkinId(m_activeSkinId), mode);
}
```

Update call sites in `activateSkin(...)`:

```cpp
    if (!loadSkinDescriptor(descriptor, SkinReloadMode::PlayStartup)) {
        return false;
    }
```

At the start of `loadSkinDescriptor`, preserve the runtime fields needed by the preserve path:

```cpp
    const bool preservePlayback = mode == SkinReloadMode::PreservePlayback;
    const QString preservedState = m_currentState;
    const QString preservedActionId = m_currentActionId;
    const QString preservedRecipeId = m_currentRecipeId;
    const int preservedRecipeStepIndex = m_currentRecipeStepIndex;
    const QString preservedPhaseId = m_currentPhaseId;
    const QString preservedFacing = m_currentFacing;
    const QString preservedMovementDirection = m_currentMovementDirection;
    const QString preservedLoopMode = m_currentLoopMode;
    const bool preservedAutoReturnToIdle = m_currentAutoReturnToIdle;
    const QUrl preservedAnimationUrl = m_currentAnimationUrl;
    const int preservedPlaybackSerial = m_playbackSerial;
```

Wrap the existing reset block in:

```cpp
    if (!preservePlayback) {
        hideCurrentProp();
        clearActiveRecipe();
        const bool soundCleared = m_audioController.clearCurrentSound();
        m_currentActionId.clear();
        m_currentPhaseId.clear();
        m_currentMovementDirection.clear();
        m_currentLoopMode = QStringLiteral("loop");
        m_currentAutoReturnToIdle = false;
        m_currentAnimationUrl.clear();
        ++m_playbackSerial;

        if (hadAction) {
            emit currentActionChanged();
        }
        if (hadPhase) {
            emit currentPhaseChanged();
        }
        if (hadMovementDirection) {
            emit currentMovementDirectionChanged();
        }
        if (loopModeChanged) {
            emit currentLoopModeChanged();
        }
        if (autoReturnChanged) {
            emit currentAutoReturnToIdleChanged();
        }
        if (wasPointerInteractionEnabled != pointerInteractionEnabled()) {
            emit pointerInteractionEnabledChanged();
        }
        if (hadAnimation) {
            emit currentAnimationUrlChanged();
        }
        if (hadSound && soundCleared) {
            emit currentSoundUrlChanged();
            emit soundPlaybackSerialChanged();
        }
        if (wasSleeping != sleeping()
                || wasSleepTransitioning != sleepTransitioning()) {
            emit sleepStateChanged();
        }
        emit playbackSerialChanged();
    }
```

After `m_manifest = nextManifest; m_activeSkinId = descriptor.id; applyManifestState();`, restore current playback when preserving:

```cpp
    if (preservePlayback) {
        m_currentState = preservedState;
        m_currentActionId = preservedActionId;
        m_currentRecipeId = preservedRecipeId;
        m_currentRecipeStepIndex = preservedRecipeStepIndex;
        m_currentPhaseId = preservedPhaseId;
        if (m_manifest.facings.contains(preservedFacing)) {
            m_currentFacing = preservedFacing;
        }
        if (m_manifest.movementDirections.contains(preservedMovementDirection)) {
            m_currentMovementDirection = preservedMovementDirection;
        }
        m_currentLoopMode = preservedLoopMode;
        m_currentAutoReturnToIdle = preservedAutoReturnToIdle;
        m_currentAnimationUrl = preservedAnimationUrl;
        m_playbackSerial = preservedPlaybackSerial;

        if (!m_currentActionId.isEmpty() && !m_manifest.actions.contains(m_currentActionId)) {
            setState(QStringLiteral("idle"));
        }

        emit activeSkinChanged();
        emit skinManifestReloaded();
        return true;
    }
```

Keep the full reload tail unchanged:

```cpp
    setState(QStringLiteral("idle"));
    emit activeSkinChanged();
    emit skinManifestReloaded();
    startStartupSequence();
    return true;
```

- [ ] **Step 6: Use preserve reload from settings save**

In `apps/desktop/src/settings/SettingsController.cpp`, replace:

```cpp
        if (!m_runtime->reloadActiveSkin()) {
```

with:

```cpp
        if (!m_runtime->reloadActiveSkinPreservingPlayback()) {
```

- [ ] **Step 7: Run tests to verify pass**

Run:

```bash
cmake --build build --target PetRuntimeSmoke SettingsServiceSmoke
ctest --test-dir build --output-on-failure -R 'pet_runtime_smoke|settings_service_smoke'
```

Expected: PASS for both tests.

- [ ] **Step 8: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntimeSkin.cpp apps/desktop/src/settings/SettingsController.cpp apps/desktop/tests/pet_runtime_smoke.cpp apps/desktop/tests/settings_service_smoke.cpp
git commit -m "feat: 设置保存静默重载皮肤"
```

## Task 2: Manifest clips/frameRange 与 onceThenHold 播放

**Files:**

- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/src/pet/PetRuntimeSkin.cpp`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.h`
- Modify: `apps/desktop/src/pet/surface/PetSurfaceWindow.cpp`
- Modify: `apps/desktop/qml/PetWindow.qml`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `tests/check_phase_0_6_animation_runtime.py`
- Modify: `tests/check_phase_0_7_phase_runtime.py`

- [ ] **Step 1: Write failing manifest loader coverage**

In `apps/desktop/tests/skin_manifest_loader_smoke.cpp`, extend the temporary `manifest.json` string so it contains clips and a onceThenHold action:

```json
  "clips": {
    "thinking_enter_right": {
      "file": "skin:assets/body/idle/stand.gif",
      "frameRange": [1, 4]
    }
  },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": {
          "animation": "skin:assets/body/idle/stand.gif"
        }
      }
    },
    "objecting": {
      "loopMode": "onceThenHold",
      "variants": {
        "right": {
          "clip": "thinking_enter_right"
        }
      }
    }
  }
```

After the existing `skin: action URL should resolve under the selected skin root` assertion, add:

```cpp
    const ActionDefinition objecting = manifest.actions.value(QStringLiteral("objecting"));
    require(objecting.loopMode == QStringLiteral("onceThenHold"),
            "loader should preserve onceThenHold loopMode");
    const AnimationVariant objectingVariant = objecting.variants.value(QStringLiteral("right"));
    require(objectingVariant.url.toString() == expectedAnimationUrl,
            "clip variant should resolve to the clip file URL");
    require(objectingVariant.frameStart == 0 && objectingVariant.frameEnd == 3,
            "loader should convert 1-based manifest frameRange to 0-based inclusive runtime frame range");
```

- [ ] **Step 2: Write failing runtime smoke coverage**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, after the expression mapping assertions around `runtime.submitExpressionRequest("idle", "polite", 0.0);`, add:

```cpp
    runtime.playAction("objecting");
    require(runtime.currentLoopMode() == "onceThenHold",
            "objecting should use onceThenHold for entry-only chat animation");
    require(!runtime.currentAutoReturnToIdle(),
            "onceThenHold must not auto-return to idle");
    int objectingCleanFinishCallbacks = 0;
    runtime.requestCleanFinishAndNotify([&objectingCleanFinishCallbacks]() {
        ++objectingCleanFinishCallbacks;
    });
    require(objectingCleanFinishCallbacks == 0,
            "onceThenHold should not clean-finish before the first playback reaches its last frame");
    runtime.handleAnimationFinished();
    require(objectingCleanFinishCallbacks == 1,
            "onceThenHold should clean-finish after it reaches the held last frame");
    require(runtime.currentActionId() == "objecting",
            "onceThenHold should stay on the entry-only action after clean finish");
```

- [ ] **Step 3: Update older contract checks to the new Phase 2.4 behavior**

In `tests/check_phase_0_6_animation_runtime.py`, replace the two objecting/bow loopMode assertions with:

```python
    require(actions["objecting"]["loopMode"] == "onceThenHold", "objecting 应播放一次后定帧")
    require(actions["bow"]["loopMode"] == "onceThenHold", "bow 应播放一次后定帧")
```

In `tests/check_phase_0_7_phase_runtime.py`, keep the sleep assertions unchanged. Add this assertion after the sleep loopMode checks:

```python
    require(actions["objecting"]["loopMode"] == "onceThenHold", "Phase 2.4 后 objecting 应是 entry-only 定帧动作")
```

- [ ] **Step 4: Run tests to verify they fail**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke|check_phase_0_6_animation_runtime|check_phase_0_7_phase_runtime'
```

Expected: FAIL because `AnimationVariant` and onceThenHold frame hold behavior are not implemented.

- [ ] **Step 5: Add manifest runtime structures**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, add:

```cpp
struct AnimationVariant
{
    QUrl url;
    int frameStart = -1;
    int frameEnd = -1;

    bool hasFrameRange() const
    {
        return frameStart >= 0 && frameEnd >= frameStart;
    }
};

struct ClipDefinition
{
    QUrl fileUrl;
    int frameStart = -1;
    int frameEnd = -1;
};
```

Change `PhaseDefinition` and `ActionDefinition` variants:

```cpp
    QHash<QString, AnimationVariant> variants;
```

Add clips to `SkinManifest`:

```cpp
    QHash<QString, ClipDefinition> clips;
```

- [ ] **Step 6: Parse clips and animation variants**

In `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`, add helper functions near the existing anonymous namespace helpers:

```cpp
QPair<int, int> frameRangeFromJson(const QJsonArray &range)
{
    if (range.size() != 2) {
        return {-1, -1};
    }

    const int startOneBased = range.at(0).toInt(0);
    const int endOneBased = range.at(1).toInt(0);
    if (startOneBased <= 0 || endOneBased < startOneBased) {
        return {-1, -1};
    }

    return {startOneBased - 1, endOneBased - 1};
}

AnimationVariant parseAnimationVariant(
    const QJsonObject &object,
    const SkinManifest &manifest,
    const QUrl &skinRootUrl
)
{
    AnimationVariant variant;

    const QString clipId = object.value(QStringLiteral("clip")).toString();
    if (!clipId.isEmpty() && manifest.clips.contains(clipId)) {
        const ClipDefinition clip = manifest.clips.value(clipId);
        variant.url = clip.fileUrl;
        variant.frameStart = clip.frameStart;
        variant.frameEnd = clip.frameEnd;
    }

    const QString animation = object.value(QStringLiteral("animation")).toString();
    if (!animation.isEmpty()) {
        variant.url = SkinManifestLoader::resolveSkinUrl(animation, skinRootUrl);
    }

    const QJsonArray localRange = object.value(QStringLiteral("frameRange")).toArray();
    const auto localFrames = frameRangeFromJson(localRange);
    if (localFrames.first >= 0) {
        variant.frameStart = localFrames.first;
        variant.frameEnd = localFrames.second;
    }

    return variant;
}
```

Before parsing actions, parse root clips:

```cpp
    const QJsonObject clips = root.value(QStringLiteral("clips")).toObject();
    for (auto it = clips.constBegin(); it != clips.constEnd(); ++it) {
        const QJsonObject clipObject = it.value().toObject();
        ClipDefinition clip;
        clip.fileUrl = resolveSkinUrl(clipObject.value(QStringLiteral("file")).toString(), manifest.skinRootUrl);
        const auto frames = frameRangeFromJson(clipObject.value(QStringLiteral("frameRange")).toArray());
        clip.frameStart = frames.first;
        clip.frameEnd = frames.second;
        if (!clip.fileUrl.isEmpty()) {
            manifest.clips.insert(it.key(), clip);
        }
    }
```

Replace action and phase variant parsing with `parseAnimationVariant(...)`, and only insert variants where `!variant.url.isEmpty()`.

- [ ] **Step 7: Expose current frame range from PetRuntime**

In `apps/desktop/src/pet/PetRuntime.h`, add properties and getters:

```cpp
    Q_PROPERTY(int currentFrameStart READ currentFrameStart NOTIFY currentAnimationUrlChanged)
    Q_PROPERTY(int currentFrameEnd READ currentFrameEnd NOTIFY currentAnimationUrlChanged)

    int currentFrameStart() const { return m_currentFrameStart; }
    int currentFrameEnd() const { return m_currentFrameEnd; }
```

Add members:

```cpp
    int m_currentFrameStart = -1;
    int m_currentFrameEnd = -1;
    bool m_currentPlaybackAtBoundary = false;
```

Update helper signatures:

```cpp
    AnimationVariant variantForFacing(const QHash<QString, AnimationVariant> &variants, const QString &facing) const;
    AnimationVariant variantForAction(const ActionDefinition &action) const;
```

In `setCurrentPhase(...)`, use the selected variant:

```cpp
    const AnimationVariant nextVariant = variantForFacing(phase.variants, m_currentFacing);
    const QUrl nextAnimationUrl = nextVariant.url;
    const int nextFrameStart = nextVariant.frameStart;
    const int nextFrameEnd = nextVariant.frameEnd;
```

Set the members:

```cpp
    m_currentFrameStart = nextFrameStart;
    m_currentFrameEnd = nextFrameEnd;
    m_currentPlaybackAtBoundary = false;
```

Treat frame range changes as animation changes:

```cpp
    const bool animationChanged = (m_currentAnimationUrl != nextAnimationUrl)
        || (m_currentFrameStart != nextFrameStart)
        || (m_currentFrameEnd != nextFrameEnd);
```

- [ ] **Step 8: Add onceThenHold behavior to native surface**

In `apps/desktop/src/pet/surface/PetSurfaceWindow.h`, add:

```cpp
    int currentEffectiveEndFrame() const;
    void jumpToFrameStartIfNeeded(int playbackSerial);
```

In `PetSurfaceWindow::restartMovieFromRuntime()`, after `m_movie->start();`, add:

```cpp
    jumpToFrameStartIfNeeded(m_runtime->playbackSerial());
```

Add implementations:

```cpp
int PetSurfaceWindow::currentEffectiveEndFrame() const
{
    const int frameEnd = m_runtime->currentFrameEnd();
    if (frameEnd >= 0) {
        return frameEnd;
    }
    return m_movie->frameCount() - 1;
}

void PetSurfaceWindow::jumpToFrameStartIfNeeded(int playbackSerial)
{
    const int frameStart = m_runtime->currentFrameStart();
    if (frameStart <= 0) {
        return;
    }

    QTimer::singleShot(0, this, [this, playbackSerial, frameStart]() {
        if (m_runtime->playbackSerial() != playbackSerial) {
            return;
        }
        if (!m_movie->jumpToFrame(frameStart)) {
            qCWarning(petRuntimeLog).noquote()
                << "QMovie jumpToFrame failed"
                << QStringLiteral("frame=%1").arg(frameStart)
                << QStringLiteral("url=%1").arg(m_runtime->currentAnimationUrl().toString());
        }
    });
}
```

In `handleMovieFrameChanged`, replace `frameCount - 1` checks with `currentEffectiveEndFrame()`. Add onceThenHold to the one-shot branch:

```cpp
    const int endFrame = currentEffectiveEndFrame();
    if (frameCount <= 0 || endFrame < 0 || frame < endFrame) {
        return;
    }

    const int playbackSerial = m_runtime->playbackSerial();
    const int currentFrameDelayMs = qMax(1, m_movie->nextFrameDelay());

    if (m_runtime->currentLoopMode() == QStringLiteral("hold")) {
        m_eventBridge->submitHoldAnimationReachedEnd();
        m_movie->setPaused(true);
        return;
    }

    if (m_runtime->currentLoopMode() == QStringLiteral("onceThenHold")) {
        m_movie->setPaused(true);
        scheduleAnimationCompletion(playbackSerial, currentFrameDelayMs);
        return;
    }
```

For `loop`, report every loop boundary and restart frameRange loops:

```cpp
    if (m_runtime->currentLoopMode() == QStringLiteral("loop")) {
        scheduleAnimationCompletion(playbackSerial, currentFrameDelayMs);
        if (m_runtime->currentFrameStart() >= 0) {
            QTimer::singleShot(currentFrameDelayMs, this, [this, playbackSerial]() {
                if (m_runtime->playbackSerial() == playbackSerial) {
                    jumpToFrameStartIfNeeded(playbackSerial);
                }
            });
        }
        if (m_runtime->currentActionAcceptsIdleLoopFinished()) {
            scheduleIdleLoopFinished(playbackSerial, currentFrameDelayMs);
        }
        return;
    }
```

- [ ] **Step 9: Keep QML surface behavior aligned**

In `apps/desktop/qml/PetWindow.qml`, add `onceThenHold` to the branch that calls `handleAnimationFinished()` at the last frame, but pause instead of returning to idle:

```qml
            if (App.PetRuntime.currentLoopMode === "onceThenHold"
                    && frameCount > 0
                    && currentFrame >= frameCount - 1) {
                pet.playing = false
                App.PetRuntime.handleAnimationFinished()
            }
```

- [ ] **Step 10: Migrate Miles manifest**

In `apps/desktop/resources/skins/miles-edgeworth/manifest.json`, change only these loop modes:

```json
    "objecting": {
      "loopMode": "onceThenHold"
    },
    "bow": {
      "loopMode": "onceThenHold"
    }
```

Do not change `crossed`, `pickup_badge`, `tea`, `turn_around`, or existing click/menu one-shot actions in this task.

- [ ] **Step 11: Run tests to verify pass**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke|check_phase_0_6_animation_runtime|check_phase_0_7_phase_runtime'
```

Expected: PASS.

- [ ] **Step 12: Commit**

```bash
git add apps/desktop/src/pet/manifest/SkinManifest.h apps/desktop/src/pet/manifest/SkinManifestLoader.cpp apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/src/pet/PetRuntimeSkin.cpp apps/desktop/src/pet/surface/PetSurfaceWindow.h apps/desktop/src/pet/surface/PetSurfaceWindow.cpp apps/desktop/qml/PetWindow.qml apps/desktop/resources/skins/miles-edgeworth/manifest.json apps/desktop/tests/skin_manifest_loader_smoke.cpp apps/desktop/tests/pet_runtime_smoke.cpp tests/check_phase_0_6_animation_runtime.py tests/check_phase_0_7_phase_runtime.py
git commit -m "feat: 支持定帧动作与帧段播放"
```

## Task 3: PetRuntime cleanFinish 单接口与自动 idle 兜底

**Files:**

- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`
- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write failing PetRuntime cleanFinish tests**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, replace the boundary notification block with cleanFinish-only coverage:

```cpp
    {
        int cleanFinishCallbacks = 0;
        runtime.playAction("bow");
        runtime.requestCleanFinishAndNotify([&cleanFinishCallbacks]() {
            ++cleanFinishCallbacks;
        });
        require(cleanFinishCallbacks == 0,
                "cleanFinish should wait for onceThenHold action to reach its held frame");
        runtime.handleAnimationFinished();
        require(cleanFinishCallbacks == 1,
                "cleanFinish should callback after onceThenHold reaches its boundary");
        require(runtime.currentActionId() == "bow",
                "cleanFinish callback postcondition should keep action finished instead of auto-idling");

        runtime.playRecipe("sleep.enterLoopExit");
        runtime.handleAnimationFinished();
        require(runtime.currentPhaseId() == "loop",
                "sleep setup should enter loop before cleanFinish");
        int sleepCleanFinishCallbacks = 0;
        runtime.requestCleanFinishAndNotify([&sleepCleanFinishCallbacks]() {
            ++sleepCleanFinishCallbacks;
        });
        runtime.handleAnimationFinished();
        require(runtime.currentPhaseId() == "exit",
                "cleanFinish from a loop phase should play the exit phase first");
        require(sleepCleanFinishCallbacks == 0,
                "cleanFinish should wait until exit finishes");
        runtime.handleAnimationFinished();
        require(sleepCleanFinishCallbacks == 1,
                "cleanFinish should callback after exit finishes");

        runtime.playAction("bow");
        int suppressedCallbacks = 0;
        runtime.setSuppressAutoIdle(true);
        runtime.requestCleanFinishAndNotify([&suppressedCallbacks]() {
            ++suppressedCallbacks;
        });
        runtime.handleAnimationFinished();
        require(suppressedCallbacks == 1,
                "suppressed cleanFinish callback should still fire");
        waitForMilliseconds(3100);
        require(runtime.currentActionId() == "bow",
                "suppressAutoIdle should prevent the 3000ms fallback idle during reply sessions");
        runtime.setSuppressAutoIdle(false);
        runtime.returnToIdle();
    }
```

- [ ] **Step 2: Update ChatController smoke away from requestBoundary**

In `apps/desktop/tests/chat_controller_smoke.cpp`, replace assertions mentioning `requestBoundaryAndNotify` with `requestCleanFinishAndNotify` wording. Keep the behavioral assertions intact.

- [ ] **Step 3: Run tests to verify they fail**

Run:

```bash
cmake --build build --target PetRuntimeSmoke ChatControllerSmoke
ctest --test-dir build --output-on-failure -R 'pet_runtime_smoke|chat_controller_smoke'
```

Expected: FAIL because `setSuppressAutoIdle` is missing and cleanFinish does not play exit.

- [ ] **Step 4: Add cleanFinish state to PetRuntime**

In `apps/desktop/src/pet/PetRuntime.h`, remove `requestBoundaryAndNotify(...)` from active use and add:

```cpp
public:
    void requestCleanFinishAndNotify(std::function<void()> callback);
    void setSuppressAutoIdle(bool suppress);

private:
    bool cleanFinishBoundaryReached() const;
    bool continueCleanFinishIfPossible();
    void triggerCleanFinishCallback();
    void clearCleanFinishCallback();
    void stopAutoIdleTimer();
```

Add members:

```cpp
    std::function<void()> m_cleanFinishCallback;
    QTimer *m_cleanFinishSafetyTimer = nullptr;
    QTimer *m_autoIdleTimer = nullptr;
    bool m_cleanFinishExitInProgress = false;
    bool m_suppressAutoIdle = false;

    static constexpr int kCleanFinishSafetyMs = 2000;
    static constexpr int kAutoIdleAfterCleanFinishMs = 3000;
```

Keep this compatibility wrapper only if older tests or external code still compile against it:

```cpp
    void requestBoundaryAndNotify(std::function<void()> callback);
```

Its implementation must delegate to `requestCleanFinishAndNotify(...)`.

- [ ] **Step 5: Implement cleanFinish flow**

In `apps/desktop/src/pet/PetRuntime.cpp`, implement:

```cpp
void PetRuntime::setSuppressAutoIdle(bool suppress)
{
    if (m_suppressAutoIdle == suppress) {
        return;
    }
    m_suppressAutoIdle = suppress;
    if (m_suppressAutoIdle) {
        stopAutoIdleTimer();
    }
}

void PetRuntime::requestBoundaryAndNotify(std::function<void()> callback)
{
    requestCleanFinishAndNotify(std::move(callback));
}

void PetRuntime::requestCleanFinishAndNotify(std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    Q_ASSERT(!m_cleanFinishCallback);
    if (m_cleanFinishCallback) {
        qCWarning(petRuntimeLog).noquote()
            << "replace pending cleanFinish callback";
        clearCleanFinishCallback();
    }

    m_cleanFinishCallback = std::move(callback);
    m_cleanFinishExitInProgress = false;

    if (m_cleanFinishSafetyTimer == nullptr) {
        m_cleanFinishSafetyTimer = new QTimer(this);
        m_cleanFinishSafetyTimer->setSingleShot(true);
        connect(m_cleanFinishSafetyTimer, &QTimer::timeout, this, [this]() {
            qCWarning(petRuntimeLog).noquote()
                << "cleanFinish safety timeout";
            triggerCleanFinishCallback();
        });
    }
    m_cleanFinishSafetyTimer->start(kCleanFinishSafetyMs);
    continueCleanFinishIfPossible();
}
```

Add boundary and callback helpers:

```cpp
bool PetRuntime::cleanFinishBoundaryReached() const
{
    if (m_currentActionId.isEmpty()) {
        return true;
    }

    const QString idleAction = actionForState(QStringLiteral("idle"));
    if (m_currentRecipeId.isEmpty()
            && m_currentState == QStringLiteral("idle")
            && !idleAction.isEmpty()
            && m_currentActionId == idleAction) {
        return true;
    }

    if (m_currentLoopMode == QStringLiteral("hold")) {
        return true;
    }

    if (m_currentLoopMode == QStringLiteral("onceThenHold")) {
        return m_currentPlaybackAtBoundary;
    }

    return m_currentPlaybackAtBoundary;
}

bool PetRuntime::continueCleanFinishIfPossible()
{
    if (!m_cleanFinishCallback || !cleanFinishBoundaryReached()) {
        return false;
    }

    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    if (!m_cleanFinishExitInProgress
            && !action.exitPhase.isEmpty()
            && m_currentPhaseId != action.exitPhase
            && action.phases.contains(action.exitPhase)) {
        m_cleanFinishExitInProgress = true;
        playPhase(m_currentActionId, action.exitPhase);
        return true;
    }

    triggerCleanFinishCallback();
    return true;
}

void PetRuntime::triggerCleanFinishCallback()
{
    if (!m_cleanFinishCallback) {
        return;
    }

    if (m_cleanFinishSafetyTimer != nullptr) {
        m_cleanFinishSafetyTimer->stop();
    }

    std::function<void()> callback = std::move(m_cleanFinishCallback);
    m_cleanFinishCallback = nullptr;
    m_cleanFinishExitInProgress = false;

    const int serialBeforeCallback = m_playbackSerial;
    callback();

    if (m_playbackSerial == serialBeforeCallback && !m_suppressAutoIdle) {
        if (m_autoIdleTimer == nullptr) {
            m_autoIdleTimer = new QTimer(this);
            m_autoIdleTimer->setSingleShot(true);
            connect(m_autoIdleTimer, &QTimer::timeout, this, [this]() {
                returnToIdle();
            });
        }
        m_autoIdleTimer->start(kAutoIdleAfterCleanFinishMs);
    }
}

void PetRuntime::clearCleanFinishCallback()
{
    if (m_cleanFinishSafetyTimer != nullptr) {
        m_cleanFinishSafetyTimer->stop();
    }
    m_cleanFinishCallback = nullptr;
    m_cleanFinishExitInProgress = false;
}

void PetRuntime::stopAutoIdleTimer()
{
    if (m_autoIdleTimer != nullptr) {
        m_autoIdleTimer->stop();
    }
}
```

- [ ] **Step 6: Integrate cleanFinish into playback transitions**

In `setCurrentPhase(...)`, add:

```cpp
    stopAutoIdleTimer();
    m_currentPlaybackAtBoundary = false;
```

In `handleAnimationFinished()`, set the boundary before auto `nextPhase`, and let cleanFinish preempt normal finish flow:

```cpp
void PetRuntime::handleAnimationFinished()
{
    const int finishingPlaybackSerial = m_playbackSerial;
    m_currentPlaybackAtBoundary = true;

    if (continueCleanFinishIfPossible()) {
        return;
    }

    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
    }

    if (m_playbackSerial != finishingPlaybackSerial) {
        return;
    }

    ...
}
```

Remove the old `drainPendingNotifications()` branch from this method after cleanFinish is in place.

- [ ] **Step 7: Suppress auto idle during reply sessions**

In `apps/desktop/src/chat/ChatController.cpp`, on `RUN_STARTED`, after `transitionTo(ChatPhase::BUFFERING_FOR_START);`, add:

```cpp
        if (m_runtime != nullptr) {
            m_runtime->setSuppressAutoIdle(true);
        }
```

In all paths that complete or isolate a reply session, call:

```cpp
    if (m_runtime != nullptr) {
        m_runtime->setSuppressAutoIdle(false);
    }
```

Apply it in `finishCurrentReply()`, `failCurrentReply(...)`, `cancelCurrentReply()`, and `isolateConversationAsyncState()` after the active reply state is cleared.

- [ ] **Step 8: Run tests to verify pass**

Run:

```bash
cmake --build build --target PetRuntimeSmoke ChatControllerSmoke
ctest --test-dir build --output-on-failure -R 'pet_runtime_smoke|chat_controller_smoke'
```

Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/src/chat/ChatController.h apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/pet_runtime_smoke.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat: 统一动画 clean finish 接口"
```

## Task 4: Runtime-controlled recipe step

**Files:**

- Modify: `apps/desktop/src/pet/manifest/SkinManifest.h`
- Modify: `apps/desktop/src/pet/manifest/SkinManifestLoader.cpp`
- Modify: `apps/desktop/src/pet/PetRuntime.h`
- Modify: `apps/desktop/src/pet/PetRuntime.cpp`
- Modify: `apps/desktop/resources/skins/miles-edgeworth/manifest.json`
- Modify: `apps/desktop/tests/skin_manifest_loader_smoke.cpp`
- Modify: `apps/desktop/tests/pet_runtime_smoke.cpp`

- [ ] **Step 1: Write failing loader coverage for `duration: "runtime"`**

In `apps/desktop/tests/skin_manifest_loader_smoke.cpp`, add a recipe to the temporary manifest:

```json
  "recipes": {
    "thinking.holdUntilCancelled": {
      "scope": "agent",
      "steps": [
        { "action": "objecting", "phase": "enter" },
        { "action": "objecting", "phase": "loop", "duration": "runtime" },
        { "action": "objecting", "phase": "exit" }
      ]
    }
  }
```

Add assertions after manifest load:

```cpp
    const RecipeDefinition thinkingRecipe = manifest.recipes.value(QStringLiteral("thinking.holdUntilCancelled"));
    require(thinkingRecipe.steps.size() == 3,
            "loader should parse runtime-controlled recipe steps");
    require(thinkingRecipe.steps.at(1).durationMode == QStringLiteral("runtime"),
            "loader should preserve duration runtime on recipe step");
```

- [ ] **Step 2: Write failing runtime coverage**

In `apps/desktop/tests/pet_runtime_smoke.cpp`, after the sleep cleanFinish block, add:

```cpp
    runtime.playRecipe("thinking.holdUntilCancelled");
    require(runtime.currentRecipeId() == "thinking.holdUntilCancelled",
            "thinking recipe should become the active recipe");
    require(runtime.currentActionId() == "thinking",
            "thinking recipe should play the thinking action");
    require(runtime.currentPhaseId() == "enter",
            "thinking recipe should start at enter phase");
    runtime.handleAnimationFinished();
    require(runtime.currentPhaseId() == "loop",
            "thinking enter step should advance to loop step");

    int thinkingCleanFinishCallbacks = 0;
    runtime.requestCleanFinishAndNotify([&thinkingCleanFinishCallbacks]() {
        ++thinkingCleanFinishCallbacks;
    });
    runtime.handleAnimationFinished();
    require(runtime.currentPhaseId() == "exit",
            "cleanFinish should end the runtime loop step and advance to exit step");
    require(thinkingCleanFinishCallbacks == 0,
            "thinking cleanFinish should wait for exit step completion");
    runtime.handleAnimationFinished();
    require(thinkingCleanFinishCallbacks == 1,
            "thinking cleanFinish should callback after exit step completes");
```

- [ ] **Step 3: Run tests to verify they fail**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke'
```

Expected: FAIL because `durationMode` and `thinking.holdUntilCancelled` are missing.

- [ ] **Step 4: Extend RecipeStep schema**

In `apps/desktop/src/pet/manifest/SkinManifest.h`, add fields:

```cpp
    QString durationMode;
    bool runtimeControlled = false;
```

Keep `durationMs` as the fixed-duration field already used by older schema.

- [ ] **Step 5: Parse `duration` in loader**

In `SkinManifestLoader.cpp`, when parsing each step, add:

```cpp
            const QJsonValue durationValue = stepObject.value(QStringLiteral("duration"));
            if (durationValue.isString()) {
                step.durationMode = durationValue.toString();
                step.runtimeControlled = step.durationMode == QStringLiteral("runtime");
            } else if (durationValue.isDouble()) {
                step.durationMs = durationValue.toInt(0);
            } else if (durationValue.isObject()) {
                step.durationMode = QStringLiteral("param");
            }
```

- [ ] **Step 6: Track runtime-controlled recipe step in PetRuntime**

In `PetRuntime.h`, add:

```cpp
    bool currentRecipeStepRuntimeControlled() const;
    bool currentRecipeHasNextStep() const;
    bool advanceRuntimeControlledRecipeStepForCleanFinish();
```

Add member:

```cpp
    bool m_currentRecipeStepRuntimeControlled = false;
```

In `playRecipeStep(...)`, set:

```cpp
    m_currentRecipeStepRuntimeControlled = step.runtimeControlled
        && step.phaseId == QStringLiteral("loop");
```

In `clearActiveRecipe()`, reset:

```cpp
    m_currentRecipeStepRuntimeControlled = false;
```

- [ ] **Step 7: Let cleanFinish end runtime loop steps**

In `PetRuntime.cpp`, add:

```cpp
bool PetRuntime::currentRecipeHasNextStep() const
{
    if (m_currentRecipeId.isEmpty() || !m_manifest.recipes.contains(m_currentRecipeId)) {
        return false;
    }
    const RecipeDefinition recipe = m_manifest.recipes.value(m_currentRecipeId);
    return m_currentRecipeStepIndex + 1 < recipe.steps.size();
}

bool PetRuntime::advanceRuntimeControlledRecipeStepForCleanFinish()
{
    if (!m_currentRecipeStepRuntimeControlled || !currentRecipeHasNextStep()) {
        return false;
    }
    m_currentRecipeStepRuntimeControlled = false;
    playNextRecipeStep();
    return true;
}
```

At the start of `continueCleanFinishIfPossible()` after boundary is reached, add:

```cpp
    if (advanceRuntimeControlledRecipeStepForCleanFinish()) {
        return true;
    }
```

In `playNextRecipeStep()`, do not clear the recipe just because the current step is a runtime-controlled loop:

```cpp
    if (isLastStep
            && !m_currentRecipeStepRuntimeControlled
            && (m_currentLoopMode == "loop" || m_currentLoopMode == "hold")) {
        clearActiveRecipe();
    }
```

- [ ] **Step 8: Add thinking phases and recipe to Miles manifest**

In `apps/desktop/resources/skins/miles-edgeworth/manifest.json`, replace the single-phase `thinking` action with phased action using the same source GIF and frame ranges:

```json
    "thinking": {
      "label": "抱胸思考",
      "category": "cognitive",
      "priority": 20,
      "tags": ["thinking", "cognitive"],
      "initialPhase": "enter",
      "exitPhase": "exit",
      "phases": {
        "enter": {
          "loopMode": "once",
          "variants": {
            "right": { "animation": "skin:assets/body/gestures/thinking-right.gif", "frameRange": [1, 4] },
            "left": { "animation": "skin:assets/body/gestures/thinking-left.gif", "frameRange": [1, 4] }
          }
        },
        "loop": {
          "loopMode": "loop",
          "variants": {
            "right": { "animation": "skin:assets/body/gestures/thinking-right.gif", "frameRange": [5, 8] },
            "left": { "animation": "skin:assets/body/gestures/thinking-left.gif", "frameRange": [5, 8] }
          }
        },
        "exit": {
          "loopMode": "onceThenHold",
          "variants": {
            "right": { "animation": "skin:assets/body/gestures/thinking-right.gif", "frameRange": [44, 47] },
            "left": { "animation": "skin:assets/body/gestures/thinking-left.gif", "frameRange": [44, 47] }
          }
        }
      }
    },
```

Add this recipe under `recipes`:

```json
    "thinking.holdUntilCancelled": {
      "label": "聊天思考直到取消",
      "scope": "agent",
      "steps": [
        { "action": "thinking", "phase": "enter" },
        { "action": "thinking", "phase": "loop", "duration": "runtime" },
        { "action": "thinking", "phase": "exit" }
      ]
    },
```

Update `expressionMappings` / action pool entry for thinking neutral so the thinking lifecycle can request this recipe:

```json
        { "recipe": "thinking.holdUntilCancelled", "allowedStates": ["thinking"] }
```

- [ ] **Step 9: Run tests to verify pass**

Run:

```bash
cmake --build build --target SkinManifestLoaderSmoke PetRuntimeSmoke
ctest --test-dir build --output-on-failure -R 'skin_manifest_loader_smoke|pet_runtime_smoke'
```

Expected: PASS.

- [ ] **Step 10: Commit**

```bash
git add apps/desktop/src/pet/manifest/SkinManifest.h apps/desktop/src/pet/manifest/SkinManifestLoader.cpp apps/desktop/src/pet/PetRuntime.h apps/desktop/src/pet/PetRuntime.cpp apps/desktop/resources/skins/miles-edgeworth/manifest.json apps/desktop/tests/skin_manifest_loader_smoke.cpp apps/desktop/tests/pet_runtime_smoke.cpp
git commit -m "feat: 支持运行时控制的动画链步骤"
```

## Task 5: SSE lifecycle 与 expression 事件拆分

**Files:**

- Modify: `apps/agent-core/internal/chat/openai/provider.go`
- Modify: `apps/agent-core/internal/chat/openai/provider_test.go`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write failing Go provider expectations**

In `apps/agent-core/internal/chat/openai/provider_test.go`, update `TestStreamChatHappyPath`:

```go
	mustFind(func(e chat.StreamEvent) bool {
		return e.Type == "CUSTOM" && e.Name == "miles.pet.lifecycle" &&
			e.Value["state"] == "thinking"
	}, "thinking lifecycle")
```

Replace the old idle custom expectation with this negative assertion after collecting `got`:

```go
	for _, e := range got {
		if e.Type == "CUSTOM" && e.Name == "miles.pet.expression.requested" && e.Value["state"] == "idle" {
			t.Fatalf("stream end must not emit idle expression event: %+v", got)
		}
	}
```

- [ ] **Step 2: Add Qt parser coverage for lifecycle event shape**

In `apps/desktop/tests/chat_stream_event_parser_smoke.cpp`, add after the existing custom expression parse block:

```cpp
    events = parser.ingest(
        "event: message\n"
        "data: {\"type\":\"CUSTOM\",\"name\":\"miles.pet.lifecycle\","
        "\"value\":{\"state\":\"thinking\"}}\n\n"
    );
    require(events.size() == 1, "lifecycle custom event should parse");
    require(events[0].name == "miles.pet.lifecycle", "lifecycle name should parse");
    require(events[0].value.value("state").toString() == "thinking", "lifecycle state should parse");
```

- [ ] **Step 3: Add ChatController lifecycle behavior coverage**

In `apps/desktop/tests/chat_controller_smoke.cpp`, replace the first thinking custom event setup:

```cpp
    thinkingEvent.name = QStringLiteral("miles.pet.lifecycle");
    thinkingEvent.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
```

Remove the `expression` value from this lifecycle event. Keep the assertions that runtime enters thinking.

- [ ] **Step 4: Run tests to verify they fail**

Run:

```bash
cd apps/agent-core && go test ./internal/chat/openai
cd ../..
cmake --build build --target ChatStreamEventParserSmoke ChatControllerSmoke
ctest --test-dir build --output-on-failure -R 'chat_stream_event_parser_smoke|chat_controller_smoke'
```

Expected: FAIL because provider still emits thinking as expression and ChatController does not handle `miles.pet.lifecycle`.

- [ ] **Step 5: Update Go provider events**

In `apps/agent-core/internal/chat/openai/provider.go`, change the initial thinking event:

```go
	if !send(ctx, events, chat.StreamEvent{
		Type:  "CUSTOM",
		Name:  "miles.pet.lifecycle",
		RunID: runID,
		Value: map[string]any{"state": "thinking"},
	}) {
		return
	}
```

Delete the idle custom event block before `RUN_FINISHED`. Keep `TEXT_MESSAGE_END` followed by `RUN_FINISHED`.

- [ ] **Step 6: Update ChatController lifecycle handling**

In `apps/desktop/src/chat/ChatController.cpp`, add:

```cpp
constexpr auto kLifecycleEvent = "miles.pet.lifecycle";
```

Before the expression event branch in `applyStreamEvent`, add:

```cpp
    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kLifecycleEvent)) {
        const QString state = event.value.value(QStringLiteral("state")).toString();
        qCDebug(chatLog).noquote() << "chat lifecycle event"
                                    << QStringLiteral("phase=%1").arg(static_cast<int>(m_phase))
                                    << QStringLiteral("state=%1").arg(state);

        if (state == QStringLiteral("thinking")) {
            if (m_phase == ChatPhase::BUFFERING_FOR_START) {
                m_pendingThinkingState = QStringLiteral("thinking");
                m_pendingThinkingExpression = QStringLiteral("neutral");
                return;
            }
            if (m_phase == ChatPhase::STREAMING && m_activeSegmentId == -1) {
                requestPetExpression(QStringLiteral("thinking"), QStringLiteral("neutral"));
                return;
            }
        }
        return;
    }
```

Add the pending thinking members in `ChatController.h`:

```cpp
    QString m_pendingThinkingState;
    QString m_pendingThinkingExpression;
```

Initialize or clear them wherever current code clears `m_pendingState` / `m_pendingExpression`.

- [ ] **Step 7: Run tests to verify pass**

Run:

```bash
cd apps/agent-core && go test ./internal/chat/openai
cd ../..
cmake --build build --target ChatStreamEventParserSmoke ChatControllerSmoke
ctest --test-dir build --output-on-failure -R 'chat_stream_event_parser_smoke|chat_controller_smoke'
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add apps/agent-core/internal/chat/openai/provider.go apps/agent-core/internal/chat/openai/provider_test.go apps/desktop/src/chat/ChatController.cpp apps/desktop/src/chat/ChatController.h apps/desktop/tests/chat_stream_event_parser_smoke.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat: 拆分聊天生命周期事件"
```

## Task 6: ChatTextPacer segmentId 与 drain 信号

**Files:**

- Modify: `apps/desktop/src/chat/ChatTextPacer.h`
- Modify: `apps/desktop/src/chat/ChatTextPacer.cpp`
- Modify: `apps/desktop/tests/chat_text_pacer_smoke.cpp`

- [ ] **Step 1: Write failing pacer segment tests**

In `apps/desktop/tests/chat_text_pacer_smoke.cpp`, add after the basic stream block:

```cpp
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(10);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);
        QSignalSpy segmentDrained(&pacer, &ChatTextPacer::segmentDrained);
        QSignalSpy pacerEmpty(&pacer, &ChatTextPacer::pacerEmpty);

        pacer.append(QStringLiteral("甲乙"), 7, 100);
        pacer.append(QStringLiteral("丙"), 7, 101);

        require(pacer.pendingCountForSegment(100) == 2,
                "segment 100 should track its own pending characters");
        require(pacer.pendingCountForSegment(101) == 1,
                "segment 101 should track its own pending characters");

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        require(drained, "segmented stream should drain");
        require(segmentDrained.size() == 2,
                "pacer should emit one segmentDrained signal per segment");
        require(segmentDrained.at(0).at(0).toInt() == 100,
                "segment 100 should drain before segment 101");
        require(segmentDrained.at(1).at(0).toInt() == 101,
                "segment 101 should drain second");
        require(pacerEmpty.size() == 1,
                "pacer should emit pacerEmpty when all text drains");
        require(assembleChunks(chunks) == QStringLiteral("甲乙丙"),
                "segmented chunks should preserve order");
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build --target ChatTextPacerSmoke
ctest --test-dir build --output-on-failure -R chat_text_pacer_smoke
```

Expected: FAIL because `segmentDrained`, `pacerEmpty`, and `pendingCountForSegment` are missing.

- [ ] **Step 3: Add segment API**

In `apps/desktop/src/chat/ChatTextPacer.h`, change:

```cpp
    void append(const QString &text, quint64 streamId = 0);
```

to:

```cpp
    void append(const QString &text, quint64 streamId = 0, int segmentId = -1);
```

Add:

```cpp
    int pendingCountForSegment(int segmentId) const;

signals:
    void chunkReady(const QString &chunk, quint64 streamId);
    void segmentDrained(int segmentId);
    void pacerEmpty();
```

Extend `QueuedChunk`:

```cpp
        int segmentId = -1;
```

- [ ] **Step 4: Implement segment drain**

In `ChatTextPacer.cpp`, update append merging:

```cpp
void ChatTextPacer::append(const QString &text, quint64 streamId, int segmentId)
{
    if (text.isEmpty()) {
        return;
    }

    if (!m_queue.isEmpty()
            && m_queue.last().streamId == streamId
            && m_queue.last().segmentId == segmentId) {
        m_queue.last().text.append(text);
    } else {
        m_queue.append(QueuedChunk{text, streamId, segmentId});
    }
    if (!m_timer.isActive()) {
        m_timer.start(effectiveInterval());
    }
}
```

Add:

```cpp
int ChatTextPacer::pendingCountForSegment(int segmentId) const
{
    int total = 0;
    for (const QueuedChunk &chunk : m_queue) {
        if (chunk.segmentId == segmentId) {
            total += chunk.text.size();
        }
    }
    return total;
}
```

In `tick()`, capture `segmentId` before removing the front chunk and emit signals after `chunkReady`:

```cpp
    const int segmentId = front.segmentId;
    ...
    const bool segmentNowDrained = segmentId >= 0 && pendingCountForSegment(segmentId) == 0;
    emit chunkReady(chunk, streamId);
    if (segmentNowDrained) {
        emit segmentDrained(segmentId);
    }
    if (isEmpty()) {
        m_timer.stop();
        emit pacerEmpty();
        return;
    }
```

In `discardBeforeStream(...)`, call `emit pacerEmpty();` when `stopIfEmpty()` empties the queue.

- [ ] **Step 5: Run test to verify pass**

Run:

```bash
cmake --build build --target ChatTextPacerSmoke
ctest --test-dir build --output-on-failure -R chat_text_pacer_smoke
```

Expected: PASS.

- [x] **Step 6: Commit**

```bash
git add apps/desktop/src/chat/ChatTextPacer.h apps/desktop/src/chat/ChatTextPacer.cpp apps/desktop/tests/chat_text_pacer_smoke.cpp
git commit -m "feat: 文字速率器支持分段 drain"
```

## Task 7: ChatController segment queue 双条件门控

**Files:**

- Modify: `apps/desktop/src/chat/ChatController.h`
- Modify: `apps/desktop/src/chat/ChatController.cpp`
- Modify: `apps/desktop/tests/chat_controller_smoke.cpp`

- [ ] **Step 1: Write failing multi-segment smoke**

In `apps/desktop/tests/chat_controller_smoke.cpp`, add this block before the stale pacer chunks tests:

```cpp
    // --- Phase 2.4: multiple expression segments must drain one by one ---
    {
        PetRuntime qRuntime;
        for (int i = 0; i < 5 && qRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            qRuntime.handleAnimationFinished();
        }
        ChatController qController(&qRuntime, &settings);

        ChatStreamEvent qStarted;
        qStarted.type = QStringLiteral("RUN_STARTED");
        qController.applyStreamEvent(qStarted);

        ChatStreamEvent qExpr1;
        qExpr1.type = QStringLiteral("CUSTOM");
        qExpr1.name = QStringLiteral("miles.pet.expression.requested");
        qExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        qExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        qController.applyStreamEvent(qExpr1);

        ChatStreamEvent qStart;
        qStart.type = QStringLiteral("TEXT_MESSAGE_START");
        qStart.role = QStringLiteral("assistant");
        qController.applyStreamEvent(qStart);

        ChatStreamEvent qText1;
        qText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        qText1.delta = QStringLiteral("第一段");
        qController.applyStreamEvent(qText1);

        ChatStreamEvent qExpr2;
        qExpr2.type = QStringLiteral("CUSTOM");
        qExpr2.name = QStringLiteral("miles.pet.expression.requested");
        qExpr2.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        qExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        qController.applyStreamEvent(qExpr2);

        ChatStreamEvent qText2;
        qText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        qText2.delta = QStringLiteral("第二段");
        qController.applyStreamEvent(qText2);

        qRuntime.handleAnimationFinished();
        require(qRuntime.currentState() == QStringLiteral("speaking"),
                "first queued expression should activate after start cleanFinish");
        require(waitFor([&qController]() {
                    return qController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("第一段");
                }),
                "first segment text should drain before second segment starts");

        require(qRuntime.currentActionId() != QStringLiteral("bow"),
                "second expression must not activate before first segment text drains and cleanFinish is ready");

        qRuntime.handleAnimationFinished();
        require(waitFor([&qController]() {
                    return qController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("第一段第二段");
                }),
                "second segment text should drain after second expression activates");
        require(qRuntime.currentActionId() == QStringLiteral("bow"),
                "polite segment should activate bow after the dual gate opens");
    }
```

- [ ] **Step 2: Write failing final-drain smoke**

Add this block after the multi-segment block:

```cpp
    // --- Phase 2.4: RUN_FINISHED waits for pacerEmpty before idle ---
    {
        SettingsService slowSettings(settingsDir.filePath(QStringLiteral("slow-settings.json")));
        auto slowCfg = modelConfig(QStringLiteral("slow"),
                                   QStringLiteral("https://api.example.test/v1"),
                                   QStringLiteral("sk-test"),
                                   QStringLiteral("miles-test-model"));
        require(slowSettings.setModelConfig(slowCfg.name, slowCfg), "slow settings should accept config");
        slowSettings.setActiveModelConfig(slowCfg.name);
        slowSettings.setMsPerChar(80);

        PetRuntime wRuntime;
        for (int i = 0; i < 5 && wRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            wRuntime.handleAnimationFinished();
        }
        ChatController wController(&wRuntime, &slowSettings);

        ChatStreamEvent wStarted;
        wStarted.type = QStringLiteral("RUN_STARTED");
        wController.applyStreamEvent(wStarted);

        ChatStreamEvent wExpr;
        wExpr.type = QStringLiteral("CUSTOM");
        wExpr.name = QStringLiteral("miles.pet.expression.requested");
        wExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        wExpr.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        wController.applyStreamEvent(wExpr);

        ChatStreamEvent wStart;
        wStart.type = QStringLiteral("TEXT_MESSAGE_START");
        wStart.role = QStringLiteral("assistant");
        wController.applyStreamEvent(wStart);

        ChatStreamEvent wText;
        wText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        wText.delta = QStringLiteral("长长长长长");
        wController.applyStreamEvent(wText);
        wRuntime.handleAnimationFinished();

        ChatStreamEvent wFinished;
        wFinished.type = QStringLiteral("RUN_FINISHED");
        wController.applyStreamEvent(wFinished);
        wRuntime.handleAnimationFinished();

        require(wRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED cleanFinish should not idle before pacerEmpty");
        require(waitFor([&wController]() {
                    return wController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("长长长长长");
                }, 1500),
                "final text should drain after RUN_FINISHED");
        require(waitFor([&wRuntime]() {
                    return wRuntime.currentState() == QStringLiteral("idle");
                }, 1500),
                "runtime should idle after cleanFinish and pacerEmpty are both ready");
    }
```

- [ ] **Step 3: Run tests to verify they fail**

Run:

```bash
cmake --build build --target ChatControllerSmoke
ctest --test-dir build --output-on-failure -R chat_controller_smoke
```

Expected: FAIL because ChatController still has one `m_pendingExpression` and one `m_holdBuffer`.

- [ ] **Step 4: Replace pending expression fields with segment queue fields**

In `apps/desktop/src/chat/ChatController.h`, add:

```cpp
    struct ExpressionSegment
    {
        int segmentId = -1;
        QString state;
        QString expression;
        QString textBuffer;
    };

    void enqueueExpressionSegment(const QString &state, const QString &expression);
    void appendTextToCurrentTarget(const QString &text);
    void activateNextSegment();
    void enterGateForNextSegment();
    void maybeAdvanceGate();
    void maybeFinishWaitingForAnimationEnd();
    void handleSegmentDrained(int segmentId);
    void handlePacerEmpty();
    void handleStartTimeout();
    void drainQueuedSegmentsToPacer();
    void resetReplySessionState();
```

Replace:

```cpp
    QString m_holdBuffer;
    QString m_pendingState;
    QString m_pendingExpression;
```

with:

```cpp
    QString m_preExpressionBuffer;
    QList<ExpressionSegment> m_segmentQueue;
    int m_nextSegmentId = 0;
    int m_activeSegmentId = -1;
    QString m_activeState;
    QString m_activeExpression;
    bool m_streamFinished = false;
    bool m_animationReady = false;
    bool m_textDrained = false;
    bool m_pacerEmpty = true;
    QTimer m_startTimeout;
```

Change timeout constants:

```cpp
    static constexpr int kGateTimeoutMs = 2000;
    static constexpr int kStartTimeoutMs = 3000;
```

- [ ] **Step 5: Wire pacer signals and start timeout**

In the constructor, add:

```cpp
    connect(m_pacer, &ChatTextPacer::segmentDrained,
            this, &ChatController::handleSegmentDrained);
    connect(m_pacer, &ChatTextPacer::pacerEmpty,
            this, &ChatController::handlePacerEmpty);

    m_startTimeout.setSingleShot(true);
    connect(&m_startTimeout, &QTimer::timeout,
            this, &ChatController::handleStartTimeout);
```

- [ ] **Step 6: Implement segment helpers**

Add these helper implementations to `ChatController.cpp`:

```cpp
void ChatController::enqueueExpressionSegment(const QString &state, const QString &expression)
{
    ExpressionSegment segment;
    segment.segmentId = m_nextSegmentId++;
    segment.state = state.trimmed().isEmpty() ? QStringLiteral("speaking") : state.trimmed();
    segment.expression = expression.trimmed().isEmpty() ? QStringLiteral("neutral") : expression.trimmed();
    if (!m_preExpressionBuffer.isEmpty()) {
        segment.textBuffer = m_preExpressionBuffer;
        m_preExpressionBuffer.clear();
    }
    m_segmentQueue.append(segment);
}

void ChatController::appendTextToCurrentTarget(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    if (!m_segmentQueue.isEmpty()) {
        m_segmentQueue.last().textBuffer.append(text);
        return;
    }
    if (m_phase == ChatPhase::STREAMING || m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        if (m_pacer != nullptr && m_activeSegmentId >= 0) {
            m_pacer->append(text, m_currentStreamId, m_activeSegmentId);
        } else {
            appendChunkToCurrentMessage(text, m_currentStreamId);
        }
        m_pacerEmpty = false;
        return;
    }
    m_preExpressionBuffer.append(text);
}

void ChatController::activateNextSegment()
{
    if (m_segmentQueue.isEmpty()) {
        return;
    }

    const ExpressionSegment segment = m_segmentQueue.takeFirst();
    m_activeSegmentId = segment.segmentId;
    m_activeState = segment.state;
    m_activeExpression = segment.expression;
    requestPetExpression(segment.state, segment.expression);

    if (!segment.textBuffer.isEmpty()) {
        if (m_pacer != nullptr) {
            m_pacer->append(segment.textBuffer, m_currentStreamId, segment.segmentId);
        } else {
            appendChunkToCurrentMessage(segment.textBuffer, m_currentStreamId);
        }
        m_pacerEmpty = false;
    }
}
```

- [ ] **Step 7: Implement gate and waiting helpers**

Add:

```cpp
void ChatController::enterGateForNextSegment()
{
    transitionTo(ChatPhase::GATED);
    m_animationReady = false;
    m_textDrained = (m_activeSegmentId < 0)
        || (m_pacer != nullptr && m_pacer->pendingCountForSegment(m_activeSegmentId) == 0);
    m_gateTimeout.start(kGateTimeoutMs);
    requestCleanFinishForCurrentStream();
}

void ChatController::maybeAdvanceGate()
{
    if (m_phase != ChatPhase::GATED || !m_animationReady || !m_textDrained) {
        return;
    }

    m_gateTimeout.stop();
    activateNextSegment();

    if (!m_segmentQueue.isEmpty()) {
        m_animationReady = false;
        m_textDrained = (m_pacer != nullptr)
            ? m_pacer->pendingCountForSegment(m_activeSegmentId) == 0
            : true;
        m_gateTimeout.start(kGateTimeoutMs);
        requestCleanFinishForCurrentStream();
        return;
    }

    if (m_streamFinished) {
        transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
        m_animationReady = false;
        m_pacerEmpty = (m_pacer == nullptr || m_pacer->pendingCount() == 0);
        requestCleanFinishForCurrentStream();
        maybeFinishWaitingForAnimationEnd();
        return;
    }

    transitionTo(ChatPhase::STREAMING);
}

void ChatController::maybeFinishWaitingForAnimationEnd()
{
    if (m_phase != ChatPhase::WAITING_FOR_ANIMATION_END || !m_animationReady || !m_pacerEmpty) {
        return;
    }

    if (m_runtime != nullptr) {
        m_runtime->returnToIdle();
        m_runtime->setSuppressAutoIdle(false);
    }
    transitionTo(ChatPhase::IDLE);
    m_assistantMessageIndex = -1;
}
```

- [ ] **Step 8: Update callbacks and timeouts**

Replace `handleCleanFinishReady()` with:

```cpp
void ChatController::handleCleanFinishReady()
{
    if (m_phase == ChatPhase::BUFFERING_FOR_START) {
        m_startTimeout.stop();
        if (!m_segmentQueue.isEmpty()) {
            activateNextSegment();
            if (!m_segmentQueue.isEmpty()) {
                enterGateForNextSegment();
            } else if (m_streamFinished) {
                transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
                m_animationReady = false;
                m_pacerEmpty = (m_pacer == nullptr || m_pacer->pendingCount() == 0);
                requestCleanFinishForCurrentStream();
                maybeFinishWaitingForAnimationEnd();
            } else {
                transitionTo(ChatPhase::STREAMING);
            }
            return;
        }

        if (!m_pendingThinkingState.isEmpty()) {
            requestPetExpression(m_pendingThinkingState, m_pendingThinkingExpression);
            m_pendingThinkingState.clear();
            m_pendingThinkingExpression.clear();
        }

        if (m_streamFinished) {
            transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
            m_animationReady = true;
            m_pacerEmpty = (m_pacer == nullptr || m_pacer->pendingCount() == 0);
            maybeFinishWaitingForAnimationEnd();
            return;
        }

        transitionTo(ChatPhase::STREAMING);
        return;
    }

    if (m_phase == ChatPhase::GATED) {
        m_animationReady = true;
        maybeAdvanceGate();
        return;
    }

    if (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        m_animationReady = true;
        maybeFinishWaitingForAnimationEnd();
    }
}
```

Implement timeout and pacer handlers:

```cpp
void ChatController::handleStartTimeout()
{
    if (m_phase != ChatPhase::BUFFERING_FOR_START) {
        return;
    }
    qCWarning(chatLog).noquote() << "start cleanFinish timeout";
    handleCleanFinishReady();
}

void ChatController::handleGateTimeout()
{
    if (m_phase != ChatPhase::GATED) {
        return;
    }
    qCWarning(chatLog).noquote() << "gate cleanFinish timeout";
    m_animationReady = true;
    maybeAdvanceGate();
}

void ChatController::handleSegmentDrained(int segmentId)
{
    if (m_phase == ChatPhase::GATED && segmentId == m_activeSegmentId) {
        m_textDrained = true;
        maybeAdvanceGate();
    }
}

void ChatController::handlePacerEmpty()
{
    m_pacerEmpty = true;
    maybeFinishWaitingForAnimationEnd();
}
```

- [ ] **Step 9: Update event handling**

On `RUN_STARTED`, clear reply session state:

```cpp
        resetReplySessionState();
        m_streamFinished = false;
        transitionTo(ChatPhase::BUFFERING_FOR_START);
        if (m_runtime != nullptr) {
            m_runtime->setSuppressAutoIdle(true);
        }
        requestCleanFinishForCurrentStream();
        m_startTimeout.start(kStartTimeoutMs);
```

On `TEXT_MESSAGE_CONTENT`, replace the old phase branch with:

```cpp
        appendTextToCurrentTarget(event.delta);
        return;
```

On `RUN_FINISHED`, set stream finished and defer idle until queue/pacer/animation conditions are met:

```cpp
        m_streamFinished = true;
        setSending(false);
        setStatusText(idleStatusText());

        if (m_phase == ChatPhase::STREAMING && m_segmentQueue.isEmpty()) {
            transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
            m_animationReady = false;
            m_pacerEmpty = (m_pacer == nullptr || m_pacer->pendingCount() == 0);
            requestCleanFinishForCurrentStream();
            maybeFinishWaitingForAnimationEnd();
        } else if (m_phase == ChatPhase::GATED) {
            maybeAdvanceGate();
        }
        return;
```

On `miles.pet.expression.requested`, use segment queue:

```cpp
        enqueueExpressionSegment(state, expression);

        if (m_phase == ChatPhase::STREAMING) {
            enterGateForNextSegment();
        } else if (m_phase == ChatPhase::IDLE) {
            activateNextSegment();
            transitionTo(ChatPhase::STREAMING);
        }
        return;
```

In all places that append held text on cancel/error, replace `drainHoldBufferToPacer()` with:

```cpp
        drainQueuedSegmentsToPacer();
```

- [ ] **Step 10: Implement queue drain and reset**

Add:

```cpp
void ChatController::drainQueuedSegmentsToPacer()
{
    if (!m_preExpressionBuffer.isEmpty()) {
        if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
            appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
            m_assistantMessageIndex = m_messages.size() - 1;
        }
        if (m_pacer != nullptr) {
            m_pacer->append(m_preExpressionBuffer, m_currentStreamId, m_activeSegmentId);
        } else {
            appendChunkToCurrentMessage(m_preExpressionBuffer, m_currentStreamId);
        }
        m_preExpressionBuffer.clear();
    }

    while (!m_segmentQueue.isEmpty()) {
        const ExpressionSegment segment = m_segmentQueue.takeFirst();
        if (segment.textBuffer.isEmpty()) {
            continue;
        }
        if (m_pacer != nullptr) {
            m_pacer->append(segment.textBuffer, m_currentStreamId, segment.segmentId);
        } else {
            appendChunkToCurrentMessage(segment.textBuffer, m_currentStreamId);
        }
    }
    m_pacerEmpty = (m_pacer == nullptr || m_pacer->pendingCount() == 0);
}

void ChatController::resetReplySessionState()
{
    m_startTimeout.stop();
    m_gateTimeout.stop();
    m_preExpressionBuffer.clear();
    m_segmentQueue.clear();
    m_activeSegmentId = -1;
    m_activeState.clear();
    m_activeExpression.clear();
    m_streamFinished = false;
    m_animationReady = false;
    m_textDrained = false;
    m_pacerEmpty = true;
    m_pendingThinkingState.clear();
    m_pendingThinkingExpression.clear();
}
```

- [ ] **Step 11: Run tests to verify pass**

Run:

```bash
cmake --build build --target ChatControllerSmoke ChatTextPacerSmoke
ctest --test-dir build --output-on-failure -R 'chat_controller_smoke|chat_text_pacer_smoke'
```

Expected: PASS.

- [ ] **Step 12: Commit**

```bash
git add apps/desktop/src/chat/ChatController.h apps/desktop/src/chat/ChatController.cpp apps/desktop/tests/chat_controller_smoke.cpp
git commit -m "feat: 聊天动画按表达分段门控"
```

## Task 8: Phase 2.4 contract check and debt cleanup

**Files:**

- Create: `tests/check_phase_2_4_phased_animation.py`
- Modify: `CMakeLists.txt`
- Modify: `docs/v2/参考资料/技术债务与评审待办.md`
- Modify: `README.md`

- [x] **Step 1: Add Phase 2.4 contract check**

Create `tests/check_phase_2_4_phased_animation.py`:

```python
#!/usr/bin/env python3
"""Check the Phase 2.4 phased animation and chat segment contract."""

from __future__ import annotations

import json
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
    manifest = json.loads(read("apps/desktop/resources/skins/miles-edgeworth/manifest.json"))
    runtime_h = read("apps/desktop/src/pet/PetRuntime.h")
    runtime_cpp = read("apps/desktop/src/pet/PetRuntime.cpp")
    surface_cpp = read("apps/desktop/src/pet/surface/PetSurfaceWindow.cpp")
    pacer_h = read("apps/desktop/src/chat/ChatTextPacer.h")
    pacer_cpp = read("apps/desktop/src/chat/ChatTextPacer.cpp")
    controller_h = read("apps/desktop/src/chat/ChatController.h")
    controller_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    provider_go = read("apps/agent-core/internal/chat/openai/provider.go")
    provider_test = read("apps/agent-core/internal/chat/openai/provider_test.go")
    root_cmake = read("CMakeLists.txt")
    debt_doc = read("docs/v2/参考资料/技术债务与评审待办.md")
    readme = read("README.md")

    actions = manifest.get("actions", {})
    recipes = manifest.get("recipes", {})
    require(actions["objecting"]["loopMode"] == "onceThenHold", "objecting must be onceThenHold")
    require(actions["bow"]["loopMode"] == "onceThenHold", "bow must be onceThenHold")
    require(set(actions["thinking"].get("phases", {}).keys()) >= {"enter", "loop", "exit"},
            "thinking must expose enter/loop/exit phases")
    require("thinking.holdUntilCancelled" in recipes, "manifest must define thinking.holdUntilCancelled")
    require(any(step.get("duration") == "runtime"
                for step in recipes["thinking.holdUntilCancelled"].get("steps", [])),
            "thinking recipe must include runtime-controlled loop step")

    for token in [
        "reloadActiveSkinPreservingPlayback",
        "setSuppressAutoIdle",
        "m_cleanFinishCallback",
        "requestCleanFinishAndNotify",
        "cancelCleanFinishNotification",
        "kCleanFinishSafetyMs = 2000",
        "kAutoIdleAfterCleanFinishMs = 3000",
        "currentFrameStart",
        "currentFrameEnd",
    ]:
        require(token in runtime_h + runtime_cpp, f"runtime missing {token}")

    require("onceThenHold" in surface_cpp and "jumpToFrame" in surface_cpp,
            "native surface must handle onceThenHold and frameRange playback")

    for token in [
        "append(const QString &text, quint64 streamId = 0, int segmentId = -1)",
        "segmentDrained",
        "pacerEmpty",
        "pendingCountForSegment",
    ]:
        require(token in pacer_h + pacer_cpp, f"pacer missing {token}")

    for token in [
        "ExpressionSegment",
        "m_segmentQueue",
        "m_activeSegmentId",
        "m_streamFinished",
        "m_startTimeout",
        "kGateTimeoutMs = 2000",
        "kStartTimeoutMs = 3000",
        "miles.pet.lifecycle",
        "maybeAdvanceGate",
        "maybeFinishWaitingForAnimationEnd",
    ]:
        require(token in controller_h + controller_cpp, f"ChatController missing {token}")

    require('"miles.pet.lifecycle"' in provider_go, "Go provider must emit lifecycle event")
    require('"miles.pet.expression.requested"' in provider_go, "Go provider must still emit expression events")
    require('"state":         "idle"' not in provider_go, "Go provider must not emit idle expression on stream end")
    require("miles.pet.lifecycle" in provider_test, "Go provider tests must cover lifecycle")

    require("check_phase_2_4_phased_animation" in root_cmake,
            "CTest must register Phase 2.4 contract check")
    require("[P2.4]" not in debt_doc,
            "P2.4 debt entries should be removed or rewritten after implementation")
    require("Phase 2.4" in readme,
            "README should link the Phase 2.4 stage record")

    print("phase 2.4 phased animation contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [x] **Step 2: Register contract check in CMake**

In root `CMakeLists.txt`, after `check_phase_2_3_3_langfuse`, add:

```cmake
        # Phase 2.4 的 phased 动画与动画链检查：守住 onceThenHold、
        # cleanFinish、segment queue、lifecycle 事件和 runtime-controlled recipe step。
        add_test(
            NAME check_phase_2_4_phased_animation
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_2_4_phased_animation.py
        )
```

- [x] **Step 3: Clean up P2.4 debt entries**

In `docs/v2/参考资料/技术债务与评审待办.md`, replace the whole section `## AI 聊天动画编排评审待办` through the last `[P2.4]` item with:

```markdown
## AI 聊天动画编排评审待办

来源：Phase 2.3.1 联调、用户手测和 PR 评审。

状态：Phase 2.4 已按 [AI 聊天动画编排设计](../设计方案/AI%20聊天动画编排设计.md) 和 [Phase 2.4 Phased 动画与动画链](../阶段记录/Phase%202.4%20Phased%20动画与动画链.md) 收口。原待办中的 RUN_FINISHED 起始边界、单 pending expression、GATED/pacer drain、boundary/cleanFinish 语义、起始同步、安全超时、取消/错误 drain、expression 随机映射验收和固定 timeout 问题均已进入实现和 contract checks。

后续如果继续扩展 TTS、唇形同步或更复杂的 action policy，应新开独立阶段记录，不复用 Phase 2.4 的待办。
```

In `## 设置与皮肤热重载待办`, replace the `[P2.3.2 收尾 / P2.4 前置] 保存设置不应重播启动入场` item with:

```markdown
### 保存设置不应重播启动入场

状态：Phase 2.4 已通过 `reloadActiveSkinPreservingPlayback()` 收口。设置保存只刷新 persona/manifest 和 sidecar 配置，不重新触发 `startup.briefcase`。主动切换皮肤和完整重载仍保留启动入场。
```

- [x] **Step 4: Update README Phase 2 status**

In `README.md`, add this bullet under the completed mainline capabilities list after the ExpressionMapping bullet:

```markdown
- Phase 2.4 聊天动画编排已接入：thinking enter/loop/exit、onceThenHold 定帧动作、segment queue 双条件门控、lifecycle SSE 事件和 runtime-controlled recipe step。
```

Add this link under `## 文档` after `Phase 2 AI 聊天粗规划`:

```markdown
- [Phase 2.4 Phased 动画与动画链](docs/v2/阶段记录/Phase%202.4%20Phased%20动画与动画链.md)
```

- [x] **Step 5: Run contract check to verify pass**

Run:

```bash
python3 tests/check_phase_2_4_phased_animation.py
ctest --test-dir build --output-on-failure -R check_phase_2_4_phased_animation
```

Expected: PASS with `phase 2.4 phased animation contract ok`.

- [x] **Step 6: Commit**

```bash
git add tests/check_phase_2_4_phased_animation.py CMakeLists.txt docs/v2/参考资料/技术债务与评审待办.md README.md
git commit -m "test: 增加 phase 2.4 动画编排验收"
```

## Final Verification

- [x] **Step 1: Run full Qt build**

```bash
cmake --build build
```

Expected: build completes without compiler errors.

- [x] **Step 2: Run full CTest**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all registered tests pass.

- [x] **Step 3: Run Go sidecar tests**

```bash
cd apps/agent-core && go test ./...
```

Expected: all Go tests pass.

- [x] **Step 4: Run diff hygiene**

```bash
git diff --check
```

Expected: no output.

- [ ] **Step 5: Manual smoke checklist**

Run the desktop app:

```bash
open build/apps/desktop/MilesEdgeworthDesktop.app
```

Check these behaviors:

- Sending a chat message starts thinking animation, then model expression switches to speaking.
- `[EXPR:objection]第一段[EXPR:polite]第二段` displays first segment before bow starts.
- `objecting` and `bow` hold on their final frame instead of returning to idle during text output.
- Reply finish waits for visible text to drain before returning to idle.
- Cancel drains already arrived local text and clean-finishes animation.
- Saving settings/persona does not replay `startup.briefcase`.
- Right-click skin reload still performs full reload behavior.

状态：本轮自动化验证未执行；需要带 provider 配置的交互式桌面烟测，或受控本地 provider stub。

## Self-Review

Spec coverage:

- Settings reload preserve mode: Task 1.
- onceThenHold and frameRange playback: Task 2.
- cleanFinish single interface, postcondition, auto idle suppression: Task 3.
- Runtime-controlled recipe step and thinking enter/loop/exit: Task 4.
- SSE lifecycle split and idle event removal: Task 5.
- ChatTextPacer segment tracking: Task 6.
- ChatController segment queue, dual gate, start/gate timeout, pacerEmpty final wait: Task 7.
- P2.4 contract and debt cleanup: Task 8.

Placeholder scan:

- No placeholder markers or unnamed test steps remain.
- Every code-changing step names exact files and includes concrete snippets.

Type consistency:

- `AnimationVariant.url/frameStart/frameEnd` is used consistently by loader, runtime, and surface.
- `ChatTextPacer::append(text, streamId, segmentId)`, `segmentDrained(int)`, and `pacerEmpty()` are used consistently by `ChatController`.
- `reloadActiveSkinPreservingPlayback()` is used only by settings save; `reloadActiveSkin()` remains the full reload path.
