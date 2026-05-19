#include "pet/PetRuntime.h"

#include "pet/behavior/BehaviorTriggerEngine.h"
#include "pet/interaction/HitZoneMatcher.h"
#include "pet/manifest/SkinManifestLoader.h"
#include "pet/selection/ActionPoolSelector.h"

#include <QRandomGenerator>
#include <QTimer>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr auto kManifestPath = ":/pet/manifest.json";
constexpr auto kFallbackActionId = "idle_stand";
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";
constexpr auto kFeedTeaCommandId = "miles.feedTea";
} // namespace

PetRuntime::PetRuntime(QObject *parent)
    : QObject(parent)
{
    setPetSize("medium");
    m_manifest = SkinManifestLoader::loadFromResource(QString::fromUtf8(kManifestPath));

    if (m_manifest.actions.isEmpty()) {
        m_manifest = SkinManifestLoader::fallbackManifest();
    }

    m_currentFacing = m_manifest.defaultFacing;
    if (!m_manifest.movementDirections.isEmpty()) {
        m_currentMovementDirection = m_manifest.movementDirections.constFirst();
    }

    setState("idle");
    startStartupSequence();
}

QString PetRuntime::currentState() const
{
    return m_currentState;
}

QString PetRuntime::currentActionId() const
{
    return m_currentActionId;
}

QString PetRuntime::currentRecipeId() const
{
    return m_currentRecipeId;
}

QString PetRuntime::currentPhaseId() const
{
    return m_currentPhaseId;
}

QUrl PetRuntime::currentAnimationUrl() const
{
    return m_currentAnimationUrl;
}

QUrl PetRuntime::currentSoundUrl() const
{
    return m_currentSoundUrl;
}

bool PetRuntime::currentPropVisible() const
{
    return m_currentPropVisible;
}

QString PetRuntime::currentPropId() const
{
    return m_currentPropId;
}

QUrl PetRuntime::currentPropImageUrl() const
{
    return m_currentPropImageUrl;
}

double PetRuntime::currentPropStartOffsetX() const
{
    return m_currentPropStartOffset.x();
}

double PetRuntime::currentPropStartOffsetY() const
{
    return m_currentPropStartOffset.y();
}

double PetRuntime::currentPropEndOffsetX() const
{
    return m_currentPropEndOffset.x();
}

double PetRuntime::currentPropEndOffsetY() const
{
    return m_currentPropEndOffset.y();
}

double PetRuntime::currentPropWidth() const
{
    return m_currentPropWidth;
}

double PetRuntime::currentPropHeight() const
{
    return m_currentPropHeight;
}

double PetRuntime::currentPropVisualWidth() const
{
    return m_currentPropVisualWidth;
}

double PetRuntime::currentPropVisualHeight() const
{
    return m_currentPropVisualHeight;
}

int PetRuntime::currentPropDurationMs() const
{
    return m_currentPropDurationMs;
}

int PetRuntime::currentPropPlaybackSerial() const
{
    return m_currentPropPlaybackSerial;
}

QString PetRuntime::currentFacing() const
{
    return m_currentFacing;
}

QString PetRuntime::currentMovementDirection() const
{
    return m_currentMovementDirection;
}

QString PetRuntime::currentLoopMode() const
{
    return m_currentLoopMode;
}

bool PetRuntime::currentAutoReturnToIdle() const
{
    return m_currentAutoReturnToIdle;
}

bool PetRuntime::audioMuted() const
{
    return m_audioMuted;
}

QString PetRuntime::voiceLanguage() const
{
    return m_voiceLanguage;
}

bool PetRuntime::autoMovementEnabled() const
{
    return m_autoMovementEnabled;
}

QString PetRuntime::petSizeId() const
{
    return m_petSizeId;
}

double PetRuntime::petScale() const
{
    return m_petScale;
}

double PetRuntime::petWindowSize() const
{
    return 120.0 * m_petScale;
}

double PetRuntime::petImageSize() const
{
    return 100.0 * m_petScale;
}

bool PetRuntime::pointerInteractionEnabled() const
{
    return acceptsPointerInteraction();
}

bool PetRuntime::sleeping() const
{
    return m_currentActionId == "sleep" && m_currentPhaseId == "loop";
}

bool PetRuntime::sleepTransitioning() const
{
    return m_currentActionId == "sleep" && m_currentPhaseId != "loop";
}

QStringList PetRuntime::enabledSkinCommandIds() const
{
    QStringList commandIds;

    // 红茶是 Miles 皮肤的定制菜单命令，不属于所有桌宠都具备的通用能力。
    // 这里先用通用 command id 暴露给 QML，后续再迁入正式 Custom Interaction。
    if (m_currentActionId != "sleep") {
        commandIds.append(QString::fromUtf8(kFeedTeaCommandId));
    }

    return commandIds;
}

int PetRuntime::playbackSerial() const
{
    return m_playbackSerial;
}

int PetRuntime::soundPlaybackSerial() const
{
    return m_soundPlaybackSerial;
}

void PetRuntime::setState(const QString &state)
{
    QString nextState = state.trimmed();
    if (nextState.isEmpty()) {
        nextState = "idle";
    }

    QString nextAction = actionForState(nextState);
    if (nextAction.isEmpty()) {
        nextState = "idle";
        nextAction = actionForState(nextState);
    }

    if (nextAction.isEmpty()) {
        nextAction = m_manifest.fallbackAction;
    }

    const bool stateChanged = (m_currentState != nextState);
    m_currentState = nextState;

    playAction(nextAction);

    if (stateChanged) {
        emit currentStateChanged();
    }
}

void PetRuntime::setFacing(const QString &facing)
{
    const QString normalizedFacing = facing.trimmed();
    if (normalizedFacing.isEmpty() || normalizedFacing == m_currentFacing || !m_manifest.facings.contains(normalizedFacing)) {
        return;
    }

    m_currentFacing = normalizedFacing;
    emit currentFacingChanged();

    // 朝向变化后，当前 action 立即换成同动作的对应朝向 variant。
    // 这样移动系统以后只需要先更新 facing，再继续播放动作即可。
    if (!m_currentActionId.isEmpty()) {
        const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
        if (!m_currentPhaseId.isEmpty() && action.phases.contains(m_currentPhaseId)) {
            playPhase(m_currentActionId, m_currentPhaseId);
        } else {
            playActionInternal(m_currentActionId, false);
        }
    }
}

void PetRuntime::toggleFacing()
{
    if (m_currentFacing == "right" && m_manifest.facings.contains("left")) {
        setFacing("left");
        return;
    }

    setFacing("right");
}

void PetRuntime::playAction(const QString &actionId)
{
    playActionInternal(actionId, true);
}

void PetRuntime::playLocomotion(const QString &actionId, const QString &movementDirection)
{
    const QString nextMovementDirection = movementDirection.trimmed();
    if (!nextMovementDirection.isEmpty() && m_manifest.movementDirections.contains(nextMovementDirection)) {
        const bool movementDirectionChanged = (m_currentMovementDirection != nextMovementDirection);
        m_currentMovementDirection = nextMovementDirection;
        updateFacingFromMovementDirection(nextMovementDirection);
        if (movementDirectionChanged) {
            emit currentMovementDirectionChanged();
        }
    }

    playAction(actionId);
}

void PetRuntime::playRecipe(const QString &recipeId)
{
    const QString nextRecipeId = recipeId.trimmed();
    if (!m_manifest.recipes.contains(nextRecipeId)) {
        return;
    }

    const bool recipeChanged = (m_currentRecipeId != nextRecipeId);
    m_currentRecipeId = nextRecipeId;
    m_currentRecipeStepIndex = -1;

    if (recipeChanged) {
        emit currentRecipeChanged();
    }

    playSoundForRecipe(m_manifest.recipes.value(nextRecipeId));
    schedulePropForRecipe(m_manifest.recipes.value(nextRecipeId));
    playNextRecipeStep();
}

void PetRuntime::playActionFromPool(const QString &poolId)
{
    const QString normalizedPoolId = ActionPoolSelector::resolvePoolId(m_manifest.actionPools, poolId, m_voiceLanguage);
    if (!m_manifest.actionPools.contains(normalizedPoolId)) {
        return;
    }

    const ActionPoolEntry entry = ActionPoolSelector::selectEntry(m_manifest.actionPools.value(normalizedPoolId));
    if (!entry.recipeId.isEmpty() && m_manifest.recipes.contains(entry.recipeId)) {
        playRecipe(entry.recipeId);
        return;
    }

    if (!entry.actionId.isEmpty()) {
        playAction(entry.actionId);
    }
}

void PetRuntime::triggerIdle()
{
    // 随机 idle 只在“真正待机站立”时触发，避免打断睡觉、喝茶或交互动作。
    // 之后接入更完整的调度器时，这里会变成 IdleController 的入口。
    if (m_currentState != "idle" || !m_currentRecipeId.isEmpty()) {
        return;
    }

    const QString idleAction = actionForState("idle");
    if (idleAction.isEmpty() || m_currentActionId != idleAction) {
        return;
    }

    playActionFromPool("idle.random");
}

void PetRuntime::handleIdleLoopFinished()
{
    handleIdleLoopFinishedWithRoll(QRandomGenerator::global()->generateDouble());
}

void PetRuntime::handleIdleLoopFinishedForTest(double randomValue)
{
    handleIdleLoopFinishedWithRoll(randomValue);
}

QVariantMap PetRuntime::consumeFrameMovementDelta() const
{
    QVariantMap delta;
    delta.insert("dx", 0.0);
    delta.insert("dy", 0.0);

    if (!m_autoMovementEnabled) {
        return delta;
    }

    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    if (action.movementDeltas.isEmpty()) {
        return delta;
    }

    const QString movementKey = (action.category == "locomotion") ? m_currentMovementDirection : m_currentFacing;
    const QPointF movementDelta = action.movementDeltas.value(movementKey, QPointF(0, 0));
    const QPointF scaledMovementDelta = movementDelta * movementScaleFactor();
    delta.insert("dx", scaledMovementDelta.x());
    delta.insert("dy", scaledMovementDelta.y());
    return delta;
}

void PetRuntime::handlePrimaryClick(double x, double y, double width, double height)
{
    if (!acceptsPointerInteraction()) {
        return;
    }

    // 旧版在睡眠中单击无反应；在其它非站立动作中单击会先回到站立。
    // v2 先保留这个交互节奏，后续双击和高级交互再共用 Interaction Pipeline。
    if (m_currentActionId == "sleep") {
        return;
    }

    const QString idleAction = actionForState("idle");
    if (!idleAction.isEmpty() && m_currentActionId != idleAction) {
        returnToIdle();
        return;
    }

    const HitZoneMatchContext hitZoneContext {
        m_currentFacing,
        m_manifest.defaultFacing,
    };
    const QString poolId = HitZoneMatcher::clickPoolForPoint(m_manifest, hitZoneContext, x, y, width, height);
    if (!poolId.isEmpty()) {
        playActionFromPool(poolId);
    }
}

void PetRuntime::handleDoubleClick()
{
    if (!acceptsPointerInteraction()) {
        return;
    }

    // 旧版睡眠中双击等同于唤醒；其它状态下随机触发语音动作。
    if (m_currentActionId == "sleep" || m_currentPhaseId == "loop") {
        returnToIdle();
        return;
    }

    playActionFromPool("doubleClick.random");
}

void PetRuntime::handleDragStarted(double globalX)
{
    if (!acceptsPointerInteraction()) {
        return;
    }

    if (m_currentActionId == "sleep") {
        return;
    }

    // 旧版在按住拖动时统计横向来回改变方向的次数。
    // 这里记录全局 X，QML 负责把窗口坐标和鼠标局部坐标合成后传进来。
    m_dragShakeTracking = true;
    m_dragShakeX = globalX;
    m_dragShakeDirection = 1;
    m_dragShakeTurns = 0;
    m_dragHoldAnimationCompleted = false;
    m_dragShakeClock.restart();
}

void PetRuntime::handleDragMoved(double globalX)
{
    if (!acceptsPointerInteraction()) {
        return;
    }

    if (!m_dragShakeTracking || m_currentActionId == "sleep") {
        return;
    }

    if (m_dragShakeClock.isValid() && m_dragShakeClock.elapsed() > 1000) {
        m_dragShakeClock.restart();
        m_dragShakeTurns = 0;
    }

    const double movement = globalX - m_dragShakeX;
    if (qFuzzyIsNull(movement)) {
        return;
    }

    if (movement * m_dragShakeDirection < 0) {
        ++m_dragShakeTurns;
        m_dragShakeDirection = -m_dragShakeDirection;
        m_dragShakeX = globalX;
    }

    if (m_dragShakeTurns >= 5) {
        m_dragShakeTracking = false;
        m_dragShakeTurns = 0;
        m_dragHoldAnimationCompleted = false;
        playAction("drag_crouch");
    }
}

void PetRuntime::handleDragEnded()
{
    if (!acceptsPointerInteraction()) {
        m_dragShakeTracking = false;
        m_dragShakeTurns = 0;
        m_dragHoldAnimationCompleted = false;
        return;
    }

    m_dragShakeTracking = false;
    m_dragShakeTurns = 0;

    if (m_currentActionId == "drag_crouch") {
        const QString recoverAction = m_dragHoldAnimationCompleted ? "drag_stand_up_full" : "drag_stand_up_quick";
        m_dragHoldAnimationCompleted = false;
        playAction(recoverAction);
        return;
    }

    m_dragHoldAnimationCompleted = false;
}

void PetRuntime::handleHoldAnimationReachedEnd()
{
    if (m_currentActionId == "drag_crouch" && m_currentLoopMode == "hold") {
        m_dragHoldAnimationCompleted = true;
    }
}

void PetRuntime::handlePropClicked()
{
    if (!m_currentPropVisible) {
        return;
    }

    const QString nextRecipeId = m_currentPropClickedRecipeId;
    hideCurrentProp();

    if (!nextRecipeId.isEmpty()) {
        playRecipe(nextRecipeId);
    }
}

void PetRuntime::handlePropExpired()
{
    if (!m_currentPropVisible) {
        return;
    }

    const QString nextRecipeId = m_currentPropExpiredRecipeId;
    hideCurrentProp();

    if (!nextRecipeId.isEmpty()) {
        playRecipe(nextRecipeId);
    }
}

void PetRuntime::toggleAudioMuted()
{
    m_audioMuted = !m_audioMuted;
    emit audioMutedChanged();
}

void PetRuntime::setVoiceLanguage(const QString &voiceLanguage)
{
    const QString normalizedLanguage = voiceLanguage.trimmed();
    if (normalizedLanguage.isEmpty() || normalizedLanguage == m_voiceLanguage) {
        return;
    }

    if (normalizedLanguage != "jp" && normalizedLanguage != "en" && normalizedLanguage != "zh") {
        return;
    }

    m_voiceLanguage = normalizedLanguage;
    emit voiceLanguageChanged();
}

void PetRuntime::toggleAutoMovementEnabled()
{
    m_autoMovementEnabled = !m_autoMovementEnabled;
    emit autoMovementEnabledChanged();
}

void PetRuntime::setPetSize(const QString &sizeId)
{
    const QString normalizedSizeId = sizeId.trimmed();
    double nextScale = m_petScale;

    if (sizeId == "mini") {
        nextScale = 1.0;
    } else if (sizeId == "small") {
        nextScale = 1.5;
    } else if (sizeId == "medium") {
        nextScale = 2.0;
    } else if (sizeId == "big") {
        nextScale = 3.0;
    } else {
        return;
    }

    if (normalizedSizeId == m_petSizeId && qFuzzyCompare(nextScale, m_petScale)) {
        return;
    }

    m_petSizeId = normalizedSizeId;
    m_petScale = nextScale;
    emit petScaleChanged();
}

void PetRuntime::triggerSkinCommand(const QString &commandId)
{
    const QString normalizedCommandId = commandId.trimmed();
    if (normalizedCommandId != QString::fromUtf8(kFeedTeaCommandId)) {
        return;
    }

    if (!enabledSkinCommandIds().contains(normalizedCommandId)) {
        return;
    }

    playActionFromPool("menu.tea");
}

void PetRuntime::toggleSleep()
{
    // 入睡或醒来的过渡动画期间不重复切换，避免一个 GIF 尚未播完又重入。
    if (sleepTransitioning()) {
        return;
    }

    if (sleeping()) {
        returnToIdle();
        return;
    }

    playRecipe("sleep.enterLoopExit");
}

void PetRuntime::startStartupSequence()
{
    playRecipe("startup.briefcase");
}

void PetRuntime::playActionInternal(const QString &actionId, bool resetRecipe)
{
    QString nextActionId = actionId.trimmed();
    if (!m_manifest.actions.contains(nextActionId)) {
        nextActionId = m_manifest.fallbackAction;
    }

    if (!m_manifest.actions.contains(nextActionId)) {
        m_manifest = SkinManifestLoader::fallbackManifest();
        m_currentFacing = m_manifest.defaultFacing;
        if (!m_manifest.movementDirections.isEmpty()) {
            m_currentMovementDirection = m_manifest.movementDirections.constFirst();
        }
        nextActionId = m_manifest.fallbackAction;
    }

    if (resetRecipe) {
        clearActiveRecipe();
    }

    setCurrentAction(nextActionId, m_manifest.actions.value(nextActionId));
}

void PetRuntime::returnToIdle()
{
    clearActiveRecipe();

    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    if (!action.exitPhase.isEmpty() && m_currentPhaseId != action.exitPhase) {
        playPhase(m_currentActionId, action.exitPhase);
        return;
    }

    setState("idle");
}

void PetRuntime::handleAnimationFinished()
{
    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
    }

    applyFacingAfterCurrentAction(action);

    if (!m_currentRecipeId.isEmpty()) {
        const RecipeDefinition recipe = m_manifest.recipes.value(m_currentRecipeId);
        if (m_currentRecipeStepIndex + 1 < recipe.steps.size()) {
            playNextRecipeStep();
            return;
        }

        clearActiveRecipe();
    }

    if (m_currentAutoReturnToIdle) {
        const QString followUpPoolId = followUpPoolForCompletedAction(action);
        if (!followUpPoolId.isEmpty()) {
            playActionFromPool(followUpPoolId);
            return;
        }

        setState("idle");
    }
}

QString PetRuntime::actionForState(const QString &state) const
{
    return m_manifest.stateToAction.value(state);
}

QUrl PetRuntime::variantForFacing(const QHash<QString, QUrl> &variants, const QString &facing) const
{
    if (variants.contains(facing)) {
        return variants.value(facing);
    }

    if (variants.contains(m_manifest.defaultFacing)) {
        return variants.value(m_manifest.defaultFacing);
    }

    if (!variants.isEmpty()) {
        return variants.constBegin().value();
    }

    return QUrl(QString::fromUtf8(kFallbackAnimationUrl));
}

QUrl PetRuntime::variantForAction(const ActionDefinition &action) const
{
    if (action.category == "locomotion") {
        return variantForFacing(action.variants, m_currentMovementDirection);
    }

    return variantForFacing(action.variants, m_currentFacing);
}

QUrl PetRuntime::soundUrlForRecipe(const RecipeDefinition &recipe) const
{
    if (recipe.soundUrls.contains(m_voiceLanguage)) {
        return recipe.soundUrls.value(m_voiceLanguage);
    }

    if (recipe.soundUrls.contains("jp")) {
        return recipe.soundUrls.value("jp");
    }

    if (!recipe.soundUrls.isEmpty()) {
        return recipe.soundUrls.constBegin().value();
    }

    return recipe.soundUrl;
}

bool PetRuntime::acceptsPointerInteraction() const
{
    // 旧版 BRIEFCASEIN 阶段直接忽略鼠标事件。
    // 这里把同一条边界放进运行时，QML 和未来其它入口都能复用。
    return m_currentActionId != "briefcase_in";
}

void PetRuntime::playSoundForRecipe(const RecipeDefinition &recipe)
{
    if (m_audioMuted) {
        return;
    }

    const QUrl nextSoundUrl = soundUrlForRecipe(recipe);
    if (nextSoundUrl.isEmpty()) {
        return;
    }

    const bool soundChanged = (m_currentSoundUrl != nextSoundUrl);
    m_currentSoundUrl = nextSoundUrl;
    ++m_soundPlaybackSerial;

    if (soundChanged) {
        emit currentSoundUrlChanged();
    }
    emit soundPlaybackSerialChanged();
}

void PetRuntime::schedulePropForRecipe(const RecipeDefinition &recipe)
{
    if (recipe.propId.isEmpty() || !m_manifest.props.contains(recipe.propId)) {
        // 任何新的非 Prop recipe 都会取消尚未飞出的延迟 Prop。
        // 这样用户在 700ms 延迟期间触发其它动作时，不会突然冒出上一轮徽章。
        ++m_propRequestSerial;
        return;
    }

    const PropDefinition prop = m_manifest.props.value(recipe.propId);
    const QString propId = recipe.propId;
    const QString facing = m_currentFacing;
    const int requestSerial = ++m_propRequestSerial;

    // 旧版 Take that 会先播放出手动作，再延迟飞出徽章。
    // 这里先把延迟保存在皮肤配置里，后续可迁移到正式 side effect 时间线。
    QTimer::singleShot(qMax(0, prop.delayMs), this, [this, propId, facing, requestSerial]() {
        if (requestSerial != m_propRequestSerial) {
            return;
        }

        spawnPropForRecipe(propId, facing);
    });
}

void PetRuntime::spawnPropForRecipe(const QString &propId, const QString &facing)
{
    if (!m_manifest.props.contains(propId)) {
        return;
    }

    const PropDefinition prop = m_manifest.props.value(propId);
    const QPointF startOffset = scaledPropPoint(propPointForFacing(prop.startOffsets, facing));
    const QPointF travelDelta = propTravelDelta(prop, facing);

    m_currentPropVisible = true;
    m_currentPropId = prop.id;
    m_currentPropImageUrl = prop.assetUrl;
    m_currentPropStartOffset = startOffset;
    m_currentPropEndOffset = startOffset + travelDelta;
    m_currentPropWidth = scaledPropLength(prop.width > 0 ? prop.width : 94);
    m_currentPropHeight = scaledPropLength(prop.height > 0 ? prop.height : 94);
    m_currentPropVisualWidth = scaledPropLength(prop.visualWidth > 0 ? prop.visualWidth : (prop.width > 0 ? prop.width : 94));
    m_currentPropVisualHeight = scaledPropLength(prop.visualHeight > 0 ? prop.visualHeight : (prop.height > 0 ? prop.height : 94));
    m_currentPropDurationMs = prop.durationMs > 0 ? prop.durationMs : 1500;
    m_currentPropClickedRecipeId = prop.clickedRecipeId;
    m_currentPropExpiredRecipeId = prop.expiredRecipeId;
    ++m_currentPropPlaybackSerial;

    emit currentPropChanged();
    emit currentPropPlaybackSerialChanged();
}

void PetRuntime::hideCurrentProp()
{
    ++m_propRequestSerial;

    if (!m_currentPropVisible && m_currentPropId.isEmpty()) {
        return;
    }

    m_currentPropVisible = false;
    m_currentPropId.clear();
    m_currentPropImageUrl = QUrl();
    m_currentPropStartOffset = QPointF();
    m_currentPropEndOffset = QPointF();
    m_currentPropWidth = 0;
    m_currentPropHeight = 0;
    m_currentPropVisualWidth = 0;
    m_currentPropVisualHeight = 0;
    m_currentPropDurationMs = 0;
    m_currentPropClickedRecipeId.clear();
    m_currentPropExpiredRecipeId.clear();

    emit currentPropChanged();
}

void PetRuntime::clearActiveRecipe()
{
    if (m_currentRecipeId.isEmpty() && m_currentRecipeStepIndex == -1) {
        return;
    }

    m_currentRecipeId.clear();
    m_currentRecipeStepIndex = -1;
    emit currentRecipeChanged();
}

void PetRuntime::playNextRecipeStep()
{
    if (m_currentRecipeId.isEmpty() || !m_manifest.recipes.contains(m_currentRecipeId)) {
        clearActiveRecipe();
        return;
    }

    const RecipeDefinition recipe = m_manifest.recipes.value(m_currentRecipeId);
    ++m_currentRecipeStepIndex;

    if (m_currentRecipeStepIndex < 0 || m_currentRecipeStepIndex >= recipe.steps.size()) {
        clearActiveRecipe();
        return;
    }

    const bool isLastStep = (m_currentRecipeStepIndex == recipe.steps.size() - 1);
    playRecipeStep(recipe.steps.at(m_currentRecipeStepIndex));

    // 如果 recipe 的最后一步是站立、睡眠循环这类持续态，它已经接管画面了。
    // 此时清掉 recipe 标记，避免 idle timer 误以为还有一段编排没结束。
    if (isLastStep && (m_currentLoopMode == "loop" || m_currentLoopMode == "hold")) {
        clearActiveRecipe();
    }
}

void PetRuntime::playRecipeStep(const RecipeStep &step)
{
    const QString recipeFacing = resolveRecipeFacing(step.facing);
    if (!recipeFacing.isEmpty() && recipeFacing != m_currentFacing) {
        setFacing(recipeFacing);
    }

    const QString movementDirection = resolveRecipeMovementDirection(step.movementDirection);
    if (!movementDirection.isEmpty() && m_manifest.movementDirections.contains(movementDirection)) {
        const bool movementDirectionChanged = (m_currentMovementDirection != movementDirection);
        m_currentMovementDirection = movementDirection;
        updateFacingFromMovementDirection(movementDirection);
        if (movementDirectionChanged) {
            emit currentMovementDirectionChanged();
        }
    }

    if (!step.recipeId.isEmpty() && m_manifest.recipes.contains(step.recipeId)) {
        playRecipe(step.recipeId);
        return;
    }

    if (!step.phaseId.isEmpty() && !step.actionId.isEmpty()) {
        playPhase(step.actionId, step.phaseId);
        return;
    }

    if (!step.actionId.isEmpty()) {
        playActionInternal(step.actionId, false);
    }
}

void PetRuntime::handleBehaviorTriggerWithRoll(const QString &triggerId, double randomValue)
{
    if (!m_manifest.behaviorTriggers.contains(triggerId)) {
        return;
    }

    const BehaviorTriggerDefinition trigger = m_manifest.behaviorTriggers.value(triggerId);
    const BehaviorTriggerContext context {
        m_currentState,
        m_currentActionId,
        !m_currentRecipeId.isEmpty(),
    };

    if (!BehaviorTriggerEngine::matches(trigger, context)) {
        return;
    }

    const BehaviorTriggerEntry entry = BehaviorTriggerEngine::selectEntry(trigger, randomValue);
    if (entry.type == "none") {
        return;
    }

    if (entry.type == "pool" && !entry.poolId.isEmpty()) {
        playActionFromPool(entry.poolId);
        return;
    }

    if (entry.type == "recipe" && !entry.recipeId.isEmpty()) {
        playRecipe(entry.recipeId);
        return;
    }

    if (entry.type == "action" && !entry.actionId.isEmpty()) {
        playAction(entry.actionId);
    }
}

QString PetRuntime::followUpPoolForCompletedAction(const ActionDefinition &action) const
{
    if (action.category != "locomotion") {
        return {};
    }

    // 旧版走路 / 跑步播完后会继续走、继续跑、换方向或停下。
    // 这里只把“完成后去哪个候选池”写在运行时，具体概率仍交给 manifest。
    if (m_currentActionId == "walk" && m_manifest.actionPools.contains("walk.finished")) {
        return "walk.finished";
    }

    if (m_currentActionId == "run" && m_manifest.actionPools.contains("run.finished")) {
        return "run.finished";
    }

    return {};
}

QString PetRuntime::resolveRecipeMovementDirection(const QString &movementDirection) const
{
    const QString normalizedDirection = movementDirection.trimmed();
    if (normalizedDirection == "$current") {
        return m_currentMovementDirection;
    }

    return normalizedDirection;
}

QString PetRuntime::resolveRecipeFacing(const QString &facing) const
{
    const QString normalizedFacing = facing.trimmed();
    if (normalizedFacing.isEmpty() || normalizedFacing == "$current") {
        return {};
    }

    if (normalizedFacing == "$opposite") {
        // Miles 只有左右两个朝向，但这里不把名字写死。
        // 未来皮肤若提供两个 facings，也能复用同一个“反向站立”recipe。
        if (m_manifest.facings.size() == 2) {
            return (m_currentFacing == m_manifest.facings.constFirst()) ? m_manifest.facings.constLast() : m_manifest.facings.constFirst();
        }

        if (m_currentFacing == "right" && m_manifest.facings.contains("left")) {
            return "left";
        }
        if (m_currentFacing == "left" && m_manifest.facings.contains("right")) {
            return "right";
        }

        for (const QString &candidate : m_manifest.facings) {
            if (candidate != m_currentFacing) {
                return candidate;
            }
        }

        return {};
    }

    if (m_manifest.facings.contains(normalizedFacing)) {
        return normalizedFacing;
    }

    return {};
}

double PetRuntime::movementScaleFactor() const
{
    // manifest 里的移动增量按旧版默认“中”尺寸 scale=2 记录。
    // 用户切换迷你/小/大时，窗口移动步长也跟着缩放，保持旧版手感。
    return m_petScale / 2.0;
}

double PetRuntime::scaledPropLength(double length) const
{
    // Prop manifest 中的尺寸按旧版默认“中”尺寸 scale=2 记录。
    // 视觉尺寸和透明点击窗口一起缩放，避免大号/迷你桌宠下徽章显得突兀。
    return length * movementScaleFactor();
}

QPointF PetRuntime::propPointForFacing(const QHash<QString, QPointF> &points, const QString &facing) const
{
    return points.value(facing, points.value(m_manifest.defaultFacing, QPointF(0, 0)));
}

QPointF PetRuntime::scaledPropPoint(const QPointF &point) const
{
    return point * movementScaleFactor();
}

QPointF PetRuntime::propTravelDelta(const PropDefinition &prop, const QString &facing) const
{
    if (!prop.travelBaseDeltas.isEmpty() || !prop.travelPerScaleDeltas.isEmpty()) {
        const QPointF base = propPointForFacing(prop.travelBaseDeltas, facing);
        const QPointF perScale = propPointForFacing(prop.travelPerScaleDeltas, facing);
        return base + perScale * m_petScale;
    }

    return scaledPropPoint(propPointForFacing(prop.travelDeltas, facing));
}

void PetRuntime::handleIdleLoopFinishedWithRoll(double randomValue)
{
    // 站立循环完成只是一个事件入口，具体概率和目标动作交给皮肤 manifest。
    // 这样 Miles 可以保留旧版 70/30 节奏，未来其它皮肤也能配置自己的待机节奏。
    handleBehaviorTriggerWithRoll("idle.loopFinished", randomValue);
}

void PetRuntime::applyFacingAfterCurrentAction(const ActionDefinition &action)
{
    if (action.facingAfter.isEmpty()) {
        return;
    }

    const QString nextFacing = action.facingAfter.value(m_currentFacing);
    if (nextFacing.isEmpty() || nextFacing == m_currentFacing || !m_manifest.facings.contains(nextFacing)) {
        return;
    }

    // 转身动画的朝向变化要发生在动画自然结束后。
    // 这里直接更新状态，不调用 setFacing，避免在最后一帧重新加载当前转身动画。
    m_currentFacing = nextFacing;
    emit currentFacingChanged();
}

void PetRuntime::updateFacingFromMovementDirection(const QString &movementDirection)
{
    QString nextFacing;
    if (movementDirection == "east" || movementDirection == "northEast" || movementDirection == "southEast") {
        nextFacing = "right";
    } else if (movementDirection == "west" || movementDirection == "northWest" || movementDirection == "southWest") {
        nextFacing = "left";
    }

    if (nextFacing.isEmpty() || nextFacing == m_currentFacing || !m_manifest.facings.contains(nextFacing)) {
        return;
    }

    m_currentFacing = nextFacing;
    emit currentFacingChanged();
}

void PetRuntime::playPhase(const QString &actionId, const QString &phaseId)
{
    const ActionDefinition action = m_manifest.actions.value(actionId);
    if (!action.phases.contains(phaseId)) {
        return;
    }

    setCurrentPhase(actionId, phaseId, action.phases.value(phaseId));
}

void PetRuntime::setCurrentAction(const QString &actionId, const ActionDefinition &action)
{
    if (!action.phases.isEmpty()) {
        QString phaseId = action.initialPhase;
        if (phaseId.isEmpty() || !action.phases.contains(phaseId)) {
            phaseId = action.phases.constBegin().key();
        }

        playPhase(actionId, phaseId);
        return;
    }

    PhaseDefinition singlePhase;
    singlePhase.loopMode = action.loopMode;
    singlePhase.variants.insert(m_manifest.defaultFacing, variantForAction(action));
    setCurrentPhase(actionId, "single", singlePhase);
}

void PetRuntime::setCurrentPhase(const QString &actionId, const QString &phaseId, const PhaseDefinition &phase)
{
    const bool wasPointerInteractionEnabled = pointerInteractionEnabled();
    const bool wasSleeping = sleeping();
    const bool wasSleepTransitioning = sleepTransitioning();
    const QStringList previousSkinCommandIds = enabledSkinCommandIds();

    const QUrl nextAnimationUrl = variantForFacing(phase.variants, m_currentFacing);
    const QString nextLoopMode = phase.loopMode.isEmpty() ? "loop" : phase.loopMode;
    const QString nextPhaseId = phaseId.isEmpty() ? "single" : phaseId;
    const bool nextAutoReturnToIdle = (nextLoopMode == "onceThenIdle");

    const bool actionChanged = (m_currentActionId != actionId);
    const bool phaseChanged = (m_currentPhaseId != nextPhaseId);
    const bool loopModeChanged = (m_currentLoopMode != nextLoopMode);
    const bool autoReturnChanged = (m_currentAutoReturnToIdle != nextAutoReturnToIdle);
    const bool animationChanged = (m_currentAnimationUrl != nextAnimationUrl);

    m_currentActionId = actionId;
    m_currentPhaseId = nextPhaseId;
    m_currentLoopMode = nextLoopMode;
    m_currentAutoReturnToIdle = nextAutoReturnToIdle;
    m_currentAnimationUrl = nextAnimationUrl;
    ++m_playbackSerial;

    if (actionChanged) {
        emit currentActionChanged();
    }
    if (phaseChanged) {
        emit currentPhaseChanged();
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
    if (animationChanged) {
        emit currentAnimationUrlChanged();
    }
    if (wasSleeping != sleeping()
            || wasSleepTransitioning != sleepTransitioning()) {
        emit sleepStateChanged();
    }
    if (previousSkinCommandIds != enabledSkinCommandIds()) {
        emit skinCommandAvailabilityChanged();
    }
    emit playbackSerialChanged();
}
