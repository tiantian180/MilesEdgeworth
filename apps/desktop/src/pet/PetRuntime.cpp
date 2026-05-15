#include "pet/PetRuntime.h"

#include <QFile>
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

QUrl PetRuntime::currentAnimationUrl() const
{
    return m_currentAnimationUrl;
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
    setState("idle");
}

void PetRuntime::testThinking()
{
    setState("thinking");
}

void PetRuntime::testSpeaking()
{
    setState("speaking");
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
        const QString animation = actionObject.value("animation").toString();
        if (animation.isEmpty()) {
            continue;
        }

        ActionDefinition action;
        action.animationUrl = QUrl(animation);
        action.loop = actionObject.value("loop").toBool(true);
        m_actions.insert(it.key(), action);
    }
}

void PetRuntime::loadFallbackManifest()
{
    m_fallbackAction = kFallbackActionId;
    m_stateToAction.clear();
    m_actions.clear();

    m_stateToAction.insert("idle", kFallbackActionId);

    ActionDefinition fallbackAction;
    fallbackAction.animationUrl = QUrl(QString::fromUtf8(kFallbackAnimationUrl));
    fallbackAction.loop = true;
    m_actions.insert(kFallbackActionId, fallbackAction);
}

QString PetRuntime::actionForState(const QString &state) const
{
    return m_stateToAction.value(state);
}

void PetRuntime::setCurrentAction(const QString &actionId, const ActionDefinition &action)
{
    const bool actionChanged = (m_currentActionId != actionId);
    const bool animationChanged = (m_currentAnimationUrl != action.animationUrl);

    m_currentActionId = actionId;
    m_currentAnimationUrl = action.animationUrl;

    if (actionChanged) {
        emit currentActionChanged();
    }
    if (animationChanged) {
        emit currentAnimationUrlChanged();
    }
}
