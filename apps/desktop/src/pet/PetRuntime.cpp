#include "pet/PetRuntime.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace {
constexpr auto kManifestPath = ":/pet/manifest.json";
constexpr auto kFallbackActionId = "idle_stand";
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";
} // namespace

PetRuntime::PetRuntime(QObject *parent)
    : QObject(parent)
{
    loadManifest();

    if (m_actions.isEmpty()) {
        loadFallbackManifest();
    }

    setState("idle");
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

QString PetRuntime::currentFacing() const
{
    return m_currentFacing;
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
            const QString animation = variantIt.value().toObject().value("animation").toString();
            if (!animation.isEmpty()) {
                action.variants.insert(variantIt.key(), QUrl(animation));
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

        const QJsonArray steps = recipeObject.value("steps").toArray();
        for (const QJsonValue &stepValue : steps) {
            const QJsonObject stepObject = stepValue.toObject();

            RecipeStep step;
            step.actionId = stepObject.value("action").toString(recipe.actionId);
            step.phaseId = stepObject.value("phase").toString();
            step.recipeId = stepObject.value("recipe").toString();
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
    m_facings = {"right", "left"};
    m_defaultFacing = "right";
    m_currentFacing = m_defaultFacing;

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
    singlePhase.variants = action.variants;
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
