#include "pet/PetRuntime.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr auto kManifestPath = ":/pet/manifest.json";
constexpr auto kFallbackActionId = "idle_stand";
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";

QString hitZoneIdForClickPool(const QString &poolId)
{
    if (poolId == "click.upperArm") {
        return "upper_arm";
    }

    if (poolId == "click.face") {
        return "face";
    }
    if (poolId == "click.head") {
        return "head";
    }
    if (poolId == "click.forearm") {
        return "forearm";
    }
    if (poolId == "click.chest") {
        return "chest";
    }
    if (poolId == "click.belly") {
        return "belly";
    }
    if (poolId == "click.legs") {
        return "legs";
    }

    return {};
}
} // namespace

PetRuntime::PetRuntime(QObject *parent)
    : QObject(parent)
{
    loadManifest();

    if (m_actions.isEmpty()) {
        loadFallbackManifest();
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
        nextAction = m_fallbackAction;
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
    if (normalizedFacing.isEmpty() || normalizedFacing == m_currentFacing || !m_facings.contains(normalizedFacing)) {
        return;
    }

    m_currentFacing = normalizedFacing;
    emit currentFacingChanged();

    // 朝向变化后，当前 action 立即换成同动作的对应朝向 variant。
    // 这样移动系统以后只需要先更新 facing，再继续播放动作即可。
    if (!m_currentActionId.isEmpty()) {
        const ActionDefinition action = m_actions.value(m_currentActionId);
        if (!m_currentPhaseId.isEmpty() && action.phases.contains(m_currentPhaseId)) {
            playPhase(m_currentActionId, m_currentPhaseId);
        } else {
            playActionInternal(m_currentActionId, false);
        }
    }
}

void PetRuntime::toggleFacing()
{
    if (m_currentFacing == "right" && m_facings.contains("left")) {
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
    if (!nextMovementDirection.isEmpty() && m_movementDirections.contains(nextMovementDirection)) {
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
    if (!m_recipes.contains(nextRecipeId)) {
        return;
    }

    const bool recipeChanged = (m_currentRecipeId != nextRecipeId);
    m_currentRecipeId = nextRecipeId;
    m_currentRecipeStepIndex = -1;

    if (recipeChanged) {
        emit currentRecipeChanged();
    }

    playSoundForRecipe(m_recipes.value(nextRecipeId));
    playNextRecipeStep();
}

void PetRuntime::playActionFromPool(const QString &poolId)
{
    const QString normalizedPoolId = poolId.trimmed();
    if (!m_actionPools.contains(normalizedPoolId)) {
        return;
    }

    const ActionPoolEntry entry = selectActionPoolEntry(m_actionPools.value(normalizedPoolId));
    if (!entry.recipeId.isEmpty() && m_recipes.contains(entry.recipeId)) {
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

QVariantMap PetRuntime::consumeFrameMovementDelta() const
{
    QVariantMap delta;
    delta.insert("dx", 0.0);
    delta.insert("dy", 0.0);

    const ActionDefinition action = m_actions.value(m_currentActionId);
    if (action.movementDeltas.isEmpty()) {
        return delta;
    }

    const QString movementKey = (action.category == "locomotion") ? m_currentMovementDirection : m_currentFacing;
    const QPointF movementDelta = action.movementDeltas.value(movementKey, QPointF(0, 0));
    delta.insert("dx", movementDelta.x());
    delta.insert("dy", movementDelta.y());
    return delta;
}

void PetRuntime::handlePrimaryClick(double x, double y, double width, double height)
{
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

    const QString poolId = clickPoolForPoint(x, y, width, height);
    if (!poolId.isEmpty()) {
        playActionFromPool(poolId);
    }
}

void PetRuntime::handleDoubleClick()
{
    // 旧版睡眠中双击等同于唤醒；其它状态下随机触发语音动作。
    if (m_currentActionId == "sleep" || m_currentPhaseId == "loop") {
        returnToIdle();
        return;
    }

    playActionFromPool("doubleClick.random");
}

void PetRuntime::handleDragStarted(double globalX)
{
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

void PetRuntime::startStartupSequence()
{
    playRecipe("startup.briefcase");
}

void PetRuntime::playActionInternal(const QString &actionId, bool resetRecipe)
{
    QString nextActionId = actionId.trimmed();
    if (!m_actions.contains(nextActionId)) {
        nextActionId = m_fallbackAction;
    }

    if (!m_actions.contains(nextActionId)) {
        loadFallbackManifest();
        nextActionId = kFallbackActionId;
    }

    if (resetRecipe) {
        clearActiveRecipe();
    }

    setCurrentAction(nextActionId, m_actions.value(nextActionId));
}

void PetRuntime::returnToIdle()
{
    clearActiveRecipe();

    const ActionDefinition action = m_actions.value(m_currentActionId);
    if (!action.exitPhase.isEmpty() && m_currentPhaseId != action.exitPhase) {
        playPhase(m_currentActionId, action.exitPhase);
        return;
    }

    setState("idle");
}

void PetRuntime::testThinking()
{
    setState("thinking");
}

void PetRuntime::testSpeaking()
{
    testObjecting();
}

void PetRuntime::testObjecting()
{
    setState("speaking");
}

void PetRuntime::testTurn()
{
    playRecipe("turn.once");
}

void PetRuntime::testWalk()
{
    playLocomotion("walk", "east");
}

void PetRuntime::testRun()
{
    playLocomotion("run", "east");
}

void PetRuntime::testBow()
{
    playRecipe("bow.once");
}

void PetRuntime::testTea()
{
    playRecipe("tea.drinkThenBow");
}

void PetRuntime::testSleep()
{
    playRecipe("sleep.enterLoopExit");
}

void PetRuntime::handleAnimationFinished()
{
    const ActionDefinition action = m_actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
    }

    applyFacingAfterCurrentAction(action);

    if (!m_currentRecipeId.isEmpty()) {
        const RecipeDefinition recipe = m_recipes.value(m_currentRecipeId);
        if (m_currentRecipeStepIndex + 1 < recipe.steps.size()) {
            playNextRecipeStep();
            return;
        }

        clearActiveRecipe();
    }

    if (m_currentAutoReturnToIdle) {
        setState("idle");
    }
}

void PetRuntime::loadManifest()
{
    QFile file(QString::fromUtf8(kManifestPath));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject root = document.object();
    m_fallbackAction = root.value("fallbackAction").toString(kFallbackActionId);
    m_defaultFacing = root.value("defaultFacing").toString("right");
    m_currentFacing = m_defaultFacing;

    const QJsonArray facings = root.value("facings").toArray();
    if (!facings.isEmpty()) {
        m_facings.clear();
        for (const QJsonValue &value : facings) {
            const QString facing = value.toString();
            if (!facing.isEmpty()) {
                m_facings.append(facing);
            }
        }
    }

    const QJsonArray movementDirections = root.value("movementDirections").toArray();
    if (!movementDirections.isEmpty()) {
        m_movementDirections.clear();
        for (const QJsonValue &value : movementDirections) {
            const QString movementDirection = value.toString();
            if (!movementDirection.isEmpty()) {
                m_movementDirections.append(movementDirection);
            }
        }
        if (!m_movementDirections.isEmpty()) {
            m_currentMovementDirection = m_movementDirections.constFirst();
        }
    }

    const QJsonObject hitZones = root.value("hitZones").toObject();
    for (auto it = hitZones.constBegin(); it != hitZones.constEnd(); ++it) {
        const QJsonObject zoneObject = it.value().toObject();
        if (zoneObject.value("type").toString() != "rect") {
            continue;
        }

        HitZoneDefinition zone;
        zone.id = it.key();
        zone.rect = QRectF(
            zoneObject.value("x").toDouble(0),
            zoneObject.value("y").toDouble(0),
            zoneObject.value("width").toDouble(0),
            zoneObject.value("height").toDouble(0)
        );

        if (zone.rect.isValid()) {
            m_hitZones.insert(zone.id, zone);
        }
    }

    const QJsonArray singleClickPools = root.value("clickBehaviors").toObject().value("singleClick").toArray();
    for (const QJsonValue &value : singleClickPools) {
        const QString poolId = value.toString();
        if (!poolId.isEmpty()) {
            m_singleClickPools.append(poolId);
        }
    }

    const QJsonObject states = root.value("states").toObject();
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        const QString action = it.value().toObject().value("action").toString();
        if (!action.isEmpty()) {
            m_stateToAction.insert(it.key(), action);
        }
    }

    const QJsonObject actions = root.value("actions").toObject();
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QJsonObject actionObject = it.value().toObject();

        ActionDefinition action;
        action.label = actionObject.value("label").toString(it.key());
        action.category = actionObject.value("category").toString();
        action.loopMode = actionObject.value("loopMode").toString(actionObject.value("loop").toBool(true) ? "loop" : "onceThenIdle");
        action.priority = actionObject.value("priority").toInt(0);
        action.initialPhase = actionObject.value("initialPhase").toString();
        action.exitPhase = actionObject.value("exitPhase").toString();

        const QJsonArray tags = actionObject.value("tags").toArray();
        for (const QJsonValue &tag : tags) {
            const QString tagText = tag.toString();
            if (!tagText.isEmpty()) {
                action.tags.append(tagText);
            }
        }

        const QJsonObject variants = actionObject.value("variants").toObject();
        for (auto variantIt = variants.constBegin(); variantIt != variants.constEnd(); ++variantIt) {
            const QJsonObject variantObject = variantIt.value().toObject();
            const QString animation = variantObject.value("animation").toString();
            if (!animation.isEmpty()) {
                action.variants.insert(variantIt.key(), QUrl(animation));
            }

            const QJsonObject movementObject = variantObject.value("movement").toObject();
            if (movementObject.contains("dx") || movementObject.contains("dy")) {
                action.movementDeltas.insert(
                    variantIt.key(),
                    QPointF(movementObject.value("dx").toDouble(0), movementObject.value("dy").toDouble(0))
                );
            }
        }

        const QJsonObject facingAfter = actionObject.value("facingAfter").toObject();
        for (auto facingIt = facingAfter.constBegin(); facingIt != facingAfter.constEnd(); ++facingIt) {
            const QString nextFacing = facingIt.value().toString();
            if (!nextFacing.isEmpty()) {
                action.facingAfter.insert(facingIt.key(), nextFacing);
            }
        }

        // 兼容 Phase 0.5 的旧 manifest：如果 action 直接写 animation，
        // 就把它当成默认朝向的 variant。
        const QString legacyAnimation = actionObject.value("animation").toString();
        if (!legacyAnimation.isEmpty()) {
            action.variants.insert(m_defaultFacing, QUrl(legacyAnimation));
        }

        const QJsonObject phases = actionObject.value("phases").toObject();
        for (auto phaseIt = phases.constBegin(); phaseIt != phases.constEnd(); ++phaseIt) {
            const QJsonObject phaseObject = phaseIt.value().toObject();
            PhaseDefinition phase;
            phase.loopMode = phaseObject.value("loopMode").toString("loop");
            phase.nextPhase = phaseObject.value("nextPhase").toString();

            const QJsonObject phaseVariants = phaseObject.value("variants").toObject();
            for (auto variantIt = phaseVariants.constBegin(); variantIt != phaseVariants.constEnd(); ++variantIt) {
                const QString animation = variantIt.value().toObject().value("animation").toString();
                if (!animation.isEmpty()) {
                    phase.variants.insert(variantIt.key(), QUrl(animation));
                }
            }

            if (!phase.variants.isEmpty()) {
                action.phases.insert(phaseIt.key(), phase);
            }
        }

        if (!action.variants.isEmpty() || !action.phases.isEmpty()) {
            m_actions.insert(it.key(), action);
        }
    }

    const QJsonObject recipes = root.value("recipes").toObject();
    for (auto it = recipes.constBegin(); it != recipes.constEnd(); ++it) {
        const QJsonObject recipeObject = it.value().toObject();

        RecipeDefinition recipe;
        recipe.label = recipeObject.value("label").toString(it.key());
        recipe.scope = recipeObject.value("scope").toString();
        recipe.actionId = recipeObject.value("action").toString();
        recipe.soundUrl = QUrl(recipeObject.value("sound").toString());

        const QJsonArray steps = recipeObject.value("steps").toArray();
        for (const QJsonValue &stepValue : steps) {
            const QJsonObject stepObject = stepValue.toObject();

            RecipeStep step;
            step.actionId = stepObject.value("action").toString(recipe.actionId);
            step.phaseId = stepObject.value("phase").toString();
            step.recipeId = stepObject.value("recipe").toString();
            step.movementDirection = stepObject.value("movementDirection").toString(recipeObject.value("movementDirection").toString());
            step.repeat = stepObject.value("repeat").toInt(1);
            step.durationMs = stepObject.value("durationMs").toInt(0);

            if (!step.actionId.isEmpty() || !step.phaseId.isEmpty() || !step.recipeId.isEmpty()) {
                recipe.steps.append(step);
            }
        }

        // 允许最简单的 recipe 只写 action，不必为了一个动作再包一层 steps。
        if (recipe.steps.isEmpty() && !recipe.actionId.isEmpty()) {
            RecipeStep singleStep;
            singleStep.actionId = recipe.actionId;
            singleStep.movementDirection = recipeObject.value("movementDirection").toString();
            recipe.steps.append(singleStep);
        }

        if (!recipe.steps.isEmpty()) {
            m_recipes.insert(it.key(), recipe);
        }
    }

    const QJsonObject actionPools = root.value("actionPools").toObject();
    for (auto it = actionPools.constBegin(); it != actionPools.constEnd(); ++it) {
        const QJsonObject poolObject = it.value().toObject();

        ActionPoolDefinition pool;
        pool.label = poolObject.value("label").toString(it.key());

        const QJsonArray entries = poolObject.value("entries").toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entryObject = entryValue.toObject();

            ActionPoolEntry entry;
            entry.recipeId = entryObject.value("recipe").toString();
            entry.actionId = entryObject.value("action").toString();
            entry.weight = entryObject.value("weight").toInt(1);
            if (entry.weight < 1) {
                entry.weight = 1;
            }

            if (!entry.recipeId.isEmpty() || !entry.actionId.isEmpty()) {
                pool.entries.append(entry);
            }
        }

        if (!pool.entries.isEmpty()) {
            m_actionPools.insert(it.key(), pool);
        }
    }
}

void PetRuntime::loadFallbackManifest()
{
    m_fallbackAction = kFallbackActionId;
    m_stateToAction.clear();
    m_actions.clear();
    m_recipes.clear();
    m_actionPools.clear();
    m_hitZones.clear();
    m_singleClickPools.clear();
    m_facings = {"right", "left"};
    m_movementDirections = {"east", "west", "northEast", "northWest", "southEast", "southWest", "north", "south"};
    m_defaultFacing = "right";
    m_currentFacing = m_defaultFacing;
    m_currentMovementDirection = "east";

    m_stateToAction.insert("idle", kFallbackActionId);

    ActionDefinition fallbackAction;
    fallbackAction.loopMode = "loop";
    fallbackAction.variants.insert("right", QUrl(QString::fromUtf8(kFallbackAnimationUrl)));
    m_actions.insert(kFallbackActionId, fallbackAction);

    RecipeDefinition idleRecipe;
    idleRecipe.scope = "state";
    idleRecipe.actionId = kFallbackActionId;
    RecipeStep idleStep;
    idleStep.actionId = kFallbackActionId;
    idleRecipe.steps.append(idleStep);
    m_recipes.insert("idle.stand", idleRecipe);
}

QString PetRuntime::actionForState(const QString &state) const
{
    return m_stateToAction.value(state);
}

QUrl PetRuntime::variantForFacing(const QHash<QString, QUrl> &variants, const QString &facing) const
{
    if (variants.contains(facing)) {
        return variants.value(facing);
    }

    if (variants.contains(m_defaultFacing)) {
        return variants.value(m_defaultFacing);
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

QString PetRuntime::clickPoolForPoint(double x, double y, double width, double height) const
{
    if (width <= 0 || height <= 0) {
        return {};
    }

    // hitZones 以 240x240 逻辑画布描述，QML 可按实际窗口尺寸传入坐标。
    // 这样后续皮肤改 canvas 或窗口缩放时，只需要统一做一次坐标归一化。
    const QPointF logicalPoint(x * 240.0 / width, y * 240.0 / height);
    for (const QString &poolId : m_singleClickPools) {
        const QString zoneId = hitZoneIdForClickPool(poolId);
        const HitZoneDefinition zone = m_hitZones.value(zoneId);
        if (!zone.id.isEmpty() && zone.rect.contains(logicalPoint)) {
            return poolId;
        }
    }

    return {};
}

void PetRuntime::playSoundForRecipe(const RecipeDefinition &recipe)
{
    if (recipe.soundUrl.isEmpty()) {
        return;
    }

    const bool soundChanged = (m_currentSoundUrl != recipe.soundUrl);
    m_currentSoundUrl = recipe.soundUrl;
    ++m_soundPlaybackSerial;

    if (soundChanged) {
        emit currentSoundUrlChanged();
    }
    emit soundPlaybackSerialChanged();
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
    if (m_currentRecipeId.isEmpty() || !m_recipes.contains(m_currentRecipeId)) {
        clearActiveRecipe();
        return;
    }

    const RecipeDefinition recipe = m_recipes.value(m_currentRecipeId);
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
    if (!step.movementDirection.isEmpty() && m_movementDirections.contains(step.movementDirection)) {
        const bool movementDirectionChanged = (m_currentMovementDirection != step.movementDirection);
        m_currentMovementDirection = step.movementDirection;
        updateFacingFromMovementDirection(step.movementDirection);
        if (movementDirectionChanged) {
            emit currentMovementDirectionChanged();
        }
    }

    if (!step.recipeId.isEmpty() && m_recipes.contains(step.recipeId)) {
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

PetRuntime::ActionPoolEntry PetRuntime::selectActionPoolEntry(const ActionPoolDefinition &pool) const
{
    ActionPoolEntry fallbackEntry;
    if (pool.entries.isEmpty()) {
        return fallbackEntry;
    }

    int totalWeight = 0;
    for (const ActionPoolEntry &entry : pool.entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight <= 0) {
        return pool.entries.constFirst();
    }

    int cursor = QRandomGenerator::global()->bounded(totalWeight);
    for (const ActionPoolEntry &entry : pool.entries) {
        cursor -= entry.weight;
        if (cursor < 0) {
            return entry;
        }
    }

    return pool.entries.constLast();
}

void PetRuntime::applyFacingAfterCurrentAction(const ActionDefinition &action)
{
    if (action.facingAfter.isEmpty()) {
        return;
    }

    const QString nextFacing = action.facingAfter.value(m_currentFacing);
    if (nextFacing.isEmpty() || nextFacing == m_currentFacing || !m_facings.contains(nextFacing)) {
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

    if (nextFacing.isEmpty() || nextFacing == m_currentFacing || !m_facings.contains(nextFacing)) {
        return;
    }

    m_currentFacing = nextFacing;
    emit currentFacingChanged();
}

void PetRuntime::playPhase(const QString &actionId, const QString &phaseId)
{
    const ActionDefinition action = m_actions.value(actionId);
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
    singlePhase.variants.insert(m_defaultFacing, variantForAction(action));
    setCurrentPhase(actionId, "single", singlePhase);
}

void PetRuntime::setCurrentPhase(const QString &actionId, const QString &phaseId, const PhaseDefinition &phase)
{
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
    if (animationChanged) {
        emit currentAnimationUrlChanged();
    }
    emit playbackSerialChanged();
}
