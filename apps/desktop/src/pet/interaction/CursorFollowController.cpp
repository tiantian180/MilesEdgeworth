#include "CursorFollowController.h"
#include "pet/PetRuntime.h"

#include <algorithm>
#include <cmath>

CursorFollowController::CursorFollowController(PetRuntime *runtime, QObject *parent)
    : QObject(parent), m_runtime(runtime)
{
    Q_ASSERT(runtime);
    connect(runtime, &PetRuntime::skinManifestReloaded, this, &CursorFollowController::pause);
    connect(runtime, &PetRuntime::reducedMotionChanged, this, &CursorFollowController::pause);
}

void CursorFollowController::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (!enabled) pause();
    emit enabledChanged();
}

void CursorFollowController::pause()
{
    m_chasing = false;
    m_runtime->stopPointerMotion();
}

void CursorFollowController::update(const QPointF &cursor, const QRect &pet, const QRect &screen, bool blocked)
{
    const auto state = m_runtime->snapshot();
    if (!m_enabled || blocked || m_runtime->reducedMotion() || !m_runtime->autoMovementPreference()
            || !state.pointerInteractionEnabled || state.sleeping || state.sleepTransitioning
            || state.currentPropVisible || !state.currentRecipeId.isEmpty()
            || (state.currentState != QStringLiteral("idle") && !m_runtime->pointerMotionActive())
            || (!m_runtime->pointerMotionActive() && !m_runtime->currentActionAcceptsIdleLoopFinished())
            || screen.isEmpty() || pet.isEmpty() || !screen.contains(cursor.toPoint())) {
        pause();
        return;
    }

    const QPointF center(pet.x() + pet.width() / 2.0, pet.y() + pet.height() / 2.0);
    const QPointF delta = cursor - center;
    const double distance = std::hypot(delta.x(), delta.y());
    const double stopRadius = std::max(pet.width(), pet.height()) / 2.0 + 24.0;
    // Hysteresis prevents restarting the walk for small cursor movements.
    if (distance <= stopRadius + (m_runtime->pointerMotionActive() ? 4.0 : 24.0)) {
        const bool completedChase = m_chasing;
        pause();
        if (completedChase && (!m_lastArrival.isValid() || m_lastArrival.elapsed() >= 8000)) {
            m_lastArrival.start();
            emit arrived();
        }
        return;
    }
    const QPointF target = cursor - delta / distance * stopRadius
        - QPointF(pet.width() / 2.0, pet.height() / 2.0);
    const int reachableWidth = std::max(0, screen.width() - pet.width());
    const int reachableHeight = std::max(0, screen.height() - pet.height());
    const double x = reachableWidth ? std::clamp((target.x() - screen.x()) / reachableWidth, 0.0, 1.0) : 0.0;
    const double y = reachableHeight ? std::clamp((target.y() - screen.y()) / reachableHeight, 0.0, 1.0) : 0.0;
    const QPoint clampedTarget = QPointF(screen.x() + x * reachableWidth, screen.y() + y * reachableHeight).toPoint();
    if ((clampedTarget - pet.topLeft()).manhattanLength() <= 4) {
        pause();
        return;
    }
    m_runtime->setMotionScreenGeometry(screen);
    m_runtime->setMotionCurrentPosition(pet.topLeft());
    m_runtime->requestPointerMotion(x, y);
    m_chasing = m_runtime->pointerMotionActive();
}
