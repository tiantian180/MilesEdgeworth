#include "pet/PetRuntime.h"

#include "pet/interaction/InteractionPipeline.h"
#include "pet/manifest/SkinManifestLoader.h"
#include "pet/selection/ActionPoolSelector.h"

#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>
#include <QtGlobal>
#include <QVariantMap>

#include <utility>

namespace {
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";
constexpr int kBoundarySafetyMs = 1500;
} // namespace

PetRuntime::PetRuntime(QObject *parent)
    : QObject(parent)
{
    connect(&m_propController, &PropController::currentPropChanged, this, &PetRuntime::currentPropChanged);
    connect(&m_propController, &PropController::currentPropPlaybackSerialChanged, this, &PetRuntime::currentPropPlaybackSerialChanged);

    refreshAvailableSkins();
    const QString savedSkinId = QSettings()
        .value(QStringLiteral("skin/activeSkinId"), QStringLiteral("miles-edgeworth"))
        .toString();

    // 构造阶段只读取用户上次选择，不回写 QSettings。
    // 用户主动切换皮肤时才持久化，避免测试或失败回退污染真实偏好。
    if (!activateSkin(savedSkinId, false)
            && !activateSkin(QStringLiteral("miles-edgeworth"), false)) {
        m_manifest = SkinManifestLoader::fallbackManifest();
        m_activeSkinId = QStringLiteral("miles-edgeworth");
        applyManifestState();
        setState(QStringLiteral("idle"));
        startStartupSequence();
    }
}

QString PetRuntime::activeSkinId() const
{
    return m_activeSkinId;
}

RuntimeSnapshot PetRuntime::snapshot() const
{
    RuntimeSnapshot snapshot;
    snapshot.currentState = m_currentState;
    snapshot.currentActionId = m_currentActionId;
    snapshot.currentRecipeId = m_currentRecipeId;
    snapshot.currentPhaseId = m_currentPhaseId;
    snapshot.currentFacing = m_currentFacing;
    const PropState prop = m_propController.snapshot();
    snapshot.currentPropId = prop.id;
    snapshot.currentPropClickedRecipeId = prop.clickedRecipeId;
    snapshot.currentPropExpiredRecipeId = prop.expiredRecipeId;
    snapshot.currentPropVisible = prop.visible;
    snapshot.pointerInteractionEnabled = acceptsPointerInteraction();
    snapshot.sleeping = sleeping();
    snapshot.sleepTransitioning = sleepTransitioning();
    return snapshot;
}

bool PetRuntime::sleeping() const
{
    const RestCapabilityDefinition rest = m_manifest.capabilities.rest;
    return rest.enabled()
        && m_currentActionId == rest.loopActionId
        && m_currentPhaseId == QStringLiteral("loop");
}

bool PetRuntime::sleepTransitioning() const
{
    const RestCapabilityDefinition rest = m_manifest.capabilities.rest;
    return rest.enabled()
        && m_currentActionId == rest.loopActionId
        && m_currentPhaseId != QStringLiteral("loop");
}

bool PetRuntime::currentActionAcceptsIdleLoopFinished() const
{
    return !m_manifest.canvas.idleLoopActionId.isEmpty()
        && m_currentActionId == m_manifest.canvas.idleLoopActionId
        && m_currentLoopMode == QStringLiteral("loop")
        && m_currentRecipeId.isEmpty();
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
    m_propController.scheduleForRecipe(m_manifest, m_manifest.recipes.value(nextRecipeId), m_currentFacing, m_petScale);
    playNextRecipeStep();
}

void PetRuntime::playActionFromPool(const QString &poolId)
{
    const QString normalizedPoolId = ActionPoolSelector::resolvePoolId(m_manifest.actionPools, poolId, m_audioController.currentLanguageId());
    if (!m_manifest.actionPools.contains(normalizedPoolId)) {
        return;
    }

    const ActionPoolEntry entry = ActionPoolSelector::selectEntry(m_manifest.actionPools.value(normalizedPoolId));
    if (entry.request.kind != ActionRequestKind::None) {
        submitActionRequest(entry.request);
        return;
    }

    if (!entry.recipeId.isEmpty() && m_manifest.recipes.contains(entry.recipeId)) {
        playRecipe(entry.recipeId);
        return;
    }

    if (!entry.actionId.isEmpty()) {
        playAction(entry.actionId);
    }
}

void PetRuntime::submitActionRequest(const ActionRequest &request)
{
    if (request.kind == ActionRequestKind::None) {
        if (request.hideCurrentProp) {
            executeActionRequest(request);
        }
        return;
    }

    if (request.interruptHint == InterruptHint::AfterCurrent && shouldDeferActionRequest(request)) {
        m_pendingRequest = request;
        return;
    }

    if (request.interruptHint == InterruptHint::Immediate) {
        m_pendingRequest = ActionRequest::none();
    }

    executeActionRequest(request);
}

void PetRuntime::executeActionRequest(const ActionRequest &request)
{
    if (request.hideCurrentProp) {
        hideCurrentProp();
    }

    applyRequestState(request);

    switch (request.kind) {
    case ActionRequestKind::None:
        return;
    case ActionRequestKind::ActionPool:
        playActionFromPool(request.targetId);
        return;
    case ActionRequestKind::Recipe:
        playRecipe(request.targetId);
        return;
    case ActionRequestKind::Action:
        playAction(request.targetId);
        return;
    case ActionRequestKind::ReturnToIdle:
        returnToIdle();
        return;
    case ActionRequestKind::ToggleFacing:
        toggleFacing();
        return;
    case ActionRequestKind::SpawnProp:
        m_propController.spawnFromRequest(m_manifest, request.targetId, m_currentFacing, m_petScale, request.options);
        return;
    case ActionRequestKind::PlaySound:
    {
        bool soundChanged = false;
        if (m_audioController.playSound(QUrl(request.targetId), &soundChanged)) {
            if (soundChanged) {
                emit currentSoundUrlChanged();
            }
            emit soundPlaybackSerialChanged();
        }
        return;
    }
    }
}

void PetRuntime::toggleAudioMuted()
{
    m_audioController.toggleMuted();
    emit audioMutedChanged();
}

void PetRuntime::setAudioLanguage(const QString &languageId)
{
    if (m_audioController.setLanguage(languageId)) {
        emit currentAudioLanguageChanged();
    }
}

void PetRuntime::toggleAutoMovementEnabled()
{
    m_autoMovementEnabled = !m_autoMovementEnabled;
    emit autoMovementEnabledChanged();
}

void PetRuntime::setPetSize(const QString &sizeId)
{
    const QString normalizedSizeId = sizeId.trimmed();
    if (normalizedSizeId.isEmpty()) {
        return;
    }

    double nextScale = 0.0;
    for (const PetSizeDefinition &size : m_manifest.sizes) {
        if (size.id == normalizedSizeId) {
            nextScale = size.scale;
            break;
        }
    }

    if (nextScale <= 0.0) {
        return;
    }

    if (normalizedSizeId == m_petSizeId && qFuzzyCompare(nextScale, m_petScale)) {
        return;
    }

    m_petSizeId = normalizedSizeId;
    m_petScale = nextScale;
    emit petScaleChanged();
}

void PetRuntime::startStartupSequence()
{
    submitRuntimeEvent(PetEvent::runtimeStarted());
}

void PetRuntime::requestExpression(const QString &state, const QString &expression)
{
    requestExpression(state, expression, InterruptHint::Immediate);
}

void PetRuntime::requestExpression(const QString &state, const QString &expression, InterruptHint interruptHint)
{
    submitExpressionRequest(state, expression, QRandomGenerator::global()->generateDouble(), interruptHint);
}

void PetRuntime::submitExpressionRequest(const QString &state, const QString &expression, double randomValue)
{
    submitExpressionRequest(state, expression, randomValue, InterruptHint::Immediate);
}

void PetRuntime::submitExpressionRequest(
    const QString &state,
    const QString &expression,
    double randomValue,
    InterruptHint interruptHint
)
{
    const QString requestedState = state.trimmed();
    QString eventState = m_currentState;
    if (!requestedState.isEmpty() && m_manifest.stateToAction.contains(requestedState)) {
        eventState = requestedState;
    }

    if (interruptHint == InterruptHint::Immediate && eventState != m_currentState) {
        m_currentState = eventState;
        emit currentStateChanged();
    }

    submitRuntimeEvent(PetEvent::agentExpressionRequested(eventState, expression, randomValue, interruptHint));
}

void PetRuntime::requestBoundaryAndNotify(std::function<void()> callback)
{
    enqueueBoundaryNotification(std::move(callback));
}

void PetRuntime::requestCleanFinishAndNotify(std::function<void()> callback)
{
    enqueueBoundaryNotification(std::move(callback));
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

void PetRuntime::playActionInternal(const QString &actionId, bool resetRecipe)
{
    QString nextActionId = actionId.trimmed();
    if (!m_manifest.actions.contains(nextActionId)) {
        nextActionId = m_manifest.fallbackAction;
    }

    if (!m_manifest.actions.contains(nextActionId)) {
        m_manifest = SkinManifestLoader::fallbackManifest();
        applyManifestState();
        nextActionId = m_manifest.fallbackAction;
    }

    if (resetRecipe) {
        clearActiveRecipe();
    }

    setCurrentAction(nextActionId, m_manifest.actions.value(nextActionId));
}

void PetRuntime::returnToIdle()
{
    const QString idleAction = actionForState(QStringLiteral("idle"));
    if (m_currentRecipeId.isEmpty()
            && m_currentState == QStringLiteral("idle")
            && !idleAction.isEmpty()
            && m_currentActionId == idleAction) {
        return;
    }

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

    drainPendingNotifications();

    applyFacingAfterCurrentAction(action);

    if (!m_currentRecipeId.isEmpty()) {
        const RecipeDefinition recipe = m_manifest.recipes.value(m_currentRecipeId);
        if (m_currentRecipeStepIndex + 1 < recipe.steps.size()) {
            playNextRecipeStep();
            return;
        }

        clearActiveRecipe();
    }

    if (m_pendingRequest.kind != ActionRequestKind::None) {
        submitPendingActionRequest();
        return;
    }

    if (m_currentAutoReturnToIdle) {
        if (submitRuntimeEvent(PetEvent::actionCompleted(QRandomGenerator::global()->generateDouble()))) {
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

bool PetRuntime::acceptsPointerInteraction() const
{
    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    return !action.blocksPointerInteraction;
}

void PetRuntime::playSoundForRecipe(const RecipeDefinition &recipe)
{
    bool soundChanged = false;
    if (!m_audioController.playSoundForRecipe(recipe, &soundChanged)) {
        return;
    }
    if (soundChanged) {
        emit currentSoundUrlChanged();
    }
    emit soundPlaybackSerialChanged();
}

void PetRuntime::hideCurrentProp()
{
    m_propController.hide();
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

bool PetRuntime::submitRuntimeEvent(const PetEvent &event)
{
    const QList<ActionRequest> requests = InteractionPipeline::handleEvent(m_manifest, snapshot(), event);
    for (const ActionRequest &request : requests) {
        submitActionRequest(request);
    }
    return !requests.isEmpty();
}

bool PetRuntime::shouldDeferActionRequest(const ActionRequest &request) const
{
    if (request.kind == ActionRequestKind::None) {
        return false;
    }

    if (m_currentActionId.isEmpty()) {
        return false;
    }

    return m_currentAutoReturnToIdle
        || !m_currentRecipeId.isEmpty()
        || m_currentLoopMode == QStringLiteral("once");
}

void PetRuntime::submitPendingActionRequest()
{
    ActionRequest request = m_pendingRequest;
    m_pendingRequest = ActionRequest::none();
    request.interruptHint = InterruptHint::Immediate;
    executeActionRequest(request);
}

void PetRuntime::applyRequestState(const ActionRequest &request)
{
    const QString requestedState = request.petState.trimmed();
    if (requestedState.isEmpty() || !m_manifest.stateToAction.contains(requestedState)) {
        return;
    }

    if (m_currentState == requestedState) {
        return;
    }

    m_currentState = requestedState;
    emit currentStateChanged();
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
    const QString nextFacing = m_manifest.movementFacingMap.value(movementDirection);

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
    const bool replacingActiveAnimation = !m_currentActionId.isEmpty()
        && (m_currentActionId != actionId || m_currentPhaseId != phaseId);

    const bool wasPointerInteractionEnabled = pointerInteractionEnabled();
    const bool wasSleeping = sleeping();
    const bool wasSleepTransitioning = sleepTransitioning();

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
    emit playbackSerialChanged();

    if (replacingActiveAnimation) {
        drainPendingNotifications();
    }
}

bool PetRuntime::atAnimationBoundary() const
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

    return m_currentLoopMode == QStringLiteral("hold")
        || m_currentLoopMode == QStringLiteral("onceThenHold");
}

void PetRuntime::enqueueBoundaryNotification(std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    if (atAnimationBoundary()) {
        callback();
        return;
    }

    PendingNotification notification;
    notification.id = ++m_nextPendingNotificationId;
    notification.callback = std::move(callback);
    notification.timer = new QTimer(this);
    notification.timer->setSingleShot(true);

    const quint64 notificationId = notification.id;
    connect(notification.timer, &QTimer::timeout, this, [this, notificationId]() {
        triggerPendingNotification(notificationId);
    });

    notification.timer->start(kBoundarySafetyMs);
    m_pendingNotifications.append(std::move(notification));
}

bool PetRuntime::drainPendingNotifications()
{
    if (m_pendingNotifications.isEmpty()) {
        return false;
    }

    QList<PendingNotification> notifications = std::move(m_pendingNotifications);
    m_pendingNotifications.clear();

    for (PendingNotification &notification : notifications) {
        if (notification.timer != nullptr) {
            notification.timer->stop();
            notification.timer->deleteLater();
            notification.timer = nullptr;
        }
    }

    for (PendingNotification &notification : notifications) {
        if (notification.callback) {
            notification.callback();
        }
    }

    return true;
}

bool PetRuntime::triggerPendingNotification(quint64 notificationId)
{
    for (qsizetype index = 0; index < m_pendingNotifications.size(); ++index) {
        if (m_pendingNotifications.at(index).id != notificationId) {
            continue;
        }

        PendingNotification notification = std::move(m_pendingNotifications[index]);
        m_pendingNotifications.removeAt(index);
        if (notification.timer != nullptr) {
            notification.timer->stop();
            notification.timer->deleteLater();
            notification.timer = nullptr;
        }
        if (notification.callback) {
            notification.callback();
        }
        return true;
    }

    return false;
}
