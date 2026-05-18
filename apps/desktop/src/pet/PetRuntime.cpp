#include "pet/PetRuntime.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
            playAction(m_currentActionId);
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
    QString nextActionId = actionId.trimmed();
    if (!m_actions.contains(nextActionId)) {
        nextActionId = m_fallbackAction;
    }

    if (!m_actions.contains(nextActionId)) {
        loadFallbackManifest();
        nextActionId = kFallbackActionId;
    }

    setCurrentAction(nextActionId, m_actions.value(nextActionId));
}

void PetRuntime::returnToIdle()
{
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
    playAction("bow");
}

void PetRuntime::testTea()
{
    playAction("tea");
}

void PetRuntime::testSleep()
{
    playAction("sleep");
}

void PetRuntime::handleAnimationFinished()
{
    const ActionDefinition action = m_actions.value(m_currentActionId);
    const PhaseDefinition phase = action.phases.value(m_currentPhaseId);
    if (!phase.nextPhase.isEmpty() && action.phases.contains(phase.nextPhase)) {
        playPhase(m_currentActionId, phase.nextPhase);
        return;
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
        action.interruptPolicy = actionObject.value("interruptPolicy").toString("replace");
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
}

void PetRuntime::loadFallbackManifest()
{
    m_fallbackAction = kFallbackActionId;
    m_stateToAction.clear();
    m_actions.clear();
    m_facings = {"right", "left"};
    m_defaultFacing = "right";
    m_currentFacing = m_defaultFacing;

    m_stateToAction.insert("idle", kFallbackActionId);

    ActionDefinition fallbackAction;
    fallbackAction.loopMode = "loop";
    fallbackAction.variants.insert("right", QUrl(QString::fromUtf8(kFallbackAnimationUrl)));
    m_actions.insert(kFallbackActionId, fallbackAction);
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
