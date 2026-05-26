#include "pet/PetRuntime.h"

#include <QSet>
#include <QTimer>

bool PetRuntime::currentPhaseWillReachSustainedLoop() const
{
    if (m_currentActionId.isEmpty() || m_currentPhaseId.isEmpty()) {
        return false;
    }
    const ActionDefinition action = m_manifest.actions.value(m_currentActionId);
    QString phaseId = m_currentPhaseId;
    QSet<QString> visited;
    while (!phaseId.isEmpty() && !visited.contains(phaseId)) {
        if (!action.phases.contains(phaseId)) {
            return false;
        }
        visited.insert(phaseId);
        const PhaseDefinition phase = action.phases.value(phaseId);
        const QString loopMode = phase.loopMode.isEmpty() ? QStringLiteral("loop") : phase.loopMode;
        if (loopMode == QStringLiteral("loop")) {
            return true;
        }
        phaseId = phase.nextPhase;
    }
    return false;
}

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

    if (m_currentLoopMode == QStringLiteral("onceThenHold")) {
        return m_currentPlaybackAtBoundary;
    }

    if (m_currentLoopMode == QStringLiteral("hold")) {
        return true;
    }

    return m_currentPlaybackAtBoundary;
}

bool PetRuntime::continueCleanFinishIfPossible()
{
    if (!m_cleanFinishCallback || !cleanFinishBoundaryReached()) {
        return false;
    }

    if (advanceRuntimeControlledRecipeStepForCleanFinish()) {
        return true;
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
