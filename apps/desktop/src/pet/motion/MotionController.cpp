#include "MotionController.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kTickIntervalMs = 16;
constexpr double kDegreesPerDirection = 45.0;
constexpr double kPi = 3.14159265358979323846;

double clampUnit(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double nonNegative(double value)
{
    return std::max(0.0, value);
}
} // namespace

MotionController::MotionController(QObject *parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(kTickIntervalMs);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &MotionController::tick);
}

void MotionController::configure(const MotionConfig &config)
{
    m_config = config;
}

void MotionController::setScreenGeometry(const QRect &screenGeometry)
{
    m_screenGeometry = screenGeometry;
}

void MotionController::setCurrentPosition(const QPoint &petWindowPosition)
{
    m_currentPosition = petWindowPosition;
}

void MotionController::moveTo(double x, double y, const QString &mode)
{
    bool clamped = false;
    beginMove(targetForPercent(x, y, &clamped), mode, clamped);
}

void MotionController::moveBy(double dx, double dy, const QString &mode)
{
    bool clamped = false;
    beginMove(targetForDeltaPercent(dx, dy, &clamped), mode, clamped);
}

void MotionController::stop()
{
    interruptWithReason(QStringLiteral("stop"));
}

void MotionController::cancelForDrag()
{
    interruptWithReason(QStringLiteral("drag"));
}

void MotionController::tick()
{
    if (m_state != State::Moving) {
        return;
    }

    const qint64 elapsedMs = m_elapsed.isValid() ? m_elapsed.restart() : kTickIntervalMs;
    const double elapsedSeconds = std::max(0.001, static_cast<double>(elapsedMs) / 1000.0);
    const QPointF delta = QPointF(m_targetPosition) - QPointF(m_currentPosition);
    const double distance = std::hypot(delta.x(), delta.y());
    const double stepDistance = speedForMode(m_currentMode) * elapsedSeconds;

    if (distance <= m_config.snapDistance || stepDistance >= distance) {
        finishAtTarget();
        return;
    }

    const QPoint nextPosition = (QPointF(m_currentPosition)
                                 + (delta / distance) * stepDistance)
                                    .toPoint();
    if (nextPosition != m_currentPosition) {
        m_currentPosition = nextPosition;
        emit positionChanged(m_currentPosition);
    }

    const QString nextDirection = directionForVector(QPointF(m_targetPosition) - QPointF(m_currentPosition));
    if (nextDirection != m_currentDirection) {
        m_currentDirection = nextDirection;
        emit directionChanged(m_currentDirection);
    }
}

QRect MotionController::reachableGeometry() const
{
    const int petSize = static_cast<int>(std::round(nonNegative(m_config.petWindowSize * m_config.petScale)));
    const int width = std::max(0, m_screenGeometry.width() - petSize);
    const int height = std::max(0, m_screenGeometry.height() - petSize);
    return QRect(m_screenGeometry.topLeft(), QSize(width, height));
}

QPoint MotionController::targetForPercent(double x, double y, bool *clamped) const
{
    bool percentClamped = false;
    const double targetX = clampUnit(x);
    const double targetY = clampUnit(y);
    percentClamped = !qFuzzyCompare(targetX + 1.0, x + 1.0) || !qFuzzyCompare(targetY + 1.0, y + 1.0);

    const QRect reachable = reachableGeometry();
    const QPointF candidate(reachable.x() + targetX * reachable.width(),
                            reachable.y() + targetY * reachable.height());
    QPoint target = clampToReachable(candidate, clamped);
    if (clamped) {
        *clamped = *clamped || percentClamped;
    }
    return target;
}

QPoint MotionController::targetForDeltaPercent(double dx, double dy, bool *clamped) const
{
    const QRect reachable = reachableGeometry();
    const QPointF candidate(QPointF(m_currentPosition)
                            + QPointF(dx * reachable.width(), dy * reachable.height()));
    return clampToReachable(candidate, clamped);
}

QPoint MotionController::clampToReachable(const QPointF &candidate, bool *clamped) const
{
    const QRect reachable = reachableGeometry();
    const double minX = reachable.x();
    const double minY = reachable.y();
    const double maxX = reachable.x() + reachable.width();
    const double maxY = reachable.y() + reachable.height();
    const double clampedX = std::clamp(candidate.x(), minX, maxX);
    const double clampedY = std::clamp(candidate.y(), minY, maxY);
    if (clamped) {
        *clamped = !qFuzzyCompare(clampedX + 1.0, candidate.x() + 1.0)
            || !qFuzzyCompare(clampedY + 1.0, candidate.y() + 1.0);
    }
    return QPointF(clampedX, clampedY).toPoint();
}

QPointF MotionController::currentPercentPosition() const
{
    const QRect reachable = reachableGeometry();
    const double x = reachable.width() > 0
        ? (static_cast<double>(m_currentPosition.x() - reachable.x()) / reachable.width())
        : 0.0;
    const double y = reachable.height() > 0
        ? (static_cast<double>(m_currentPosition.y() - reachable.y()) / reachable.height())
        : 0.0;
    return QPointF(clampUnit(x), clampUnit(y));
}

QString MotionController::normalizedMode(const QString &mode) const
{
    return mode == QStringLiteral("run") ? QStringLiteral("run") : QStringLiteral("walk");
}

double MotionController::speedForMode(const QString &mode) const
{
    return normalizedMode(mode) == QStringLiteral("run") ? m_config.runSpeed : m_config.walkSpeed;
}

QString MotionController::directionForVector(const QPointF &vector) const
{
    static const QStringList directions = {
        QStringLiteral("east"),
        QStringLiteral("northEast"),
        QStringLiteral("north"),
        QStringLiteral("northWest"),
        QStringLiteral("west"),
        QStringLiteral("southWest"),
        QStringLiteral("south"),
        QStringLiteral("southEast"),
    };

    if (qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y())) {
        return m_currentDirection.isEmpty() ? QStringLiteral("east") : m_currentDirection;
    }

    double degrees = std::atan2(-vector.y(), vector.x()) * 180.0 / kPi;
    if (degrees < 0.0) {
        degrees += 360.0;
    }

    const int directionIndex = static_cast<int>(std::round(degrees / kDegreesPerDirection)) % directions.size();
    const QString quantized = directions.at(directionIndex);
    if (m_config.availableDirections.contains(quantized)) {
        return quantized;
    }
    if (m_config.availableDirections.contains(QStringLiteral("east"))) {
        return QStringLiteral("east");
    }
    if (!m_config.availableDirections.isEmpty()) {
        return m_config.availableDirections.first();
    }
    return QStringLiteral("east");
}

void MotionController::beginMove(const QPoint &target, const QString &mode, bool clamped)
{
    m_targetPosition = target;
    m_currentMode = normalizedMode(mode);
    m_lastMoveWasClamped = clamped;

    const QString nextDirection = directionForVector(QPointF(m_targetPosition) - QPointF(m_currentPosition));
    if (m_state == State::Moving) {
        if (nextDirection != m_currentDirection) {
            m_currentDirection = nextDirection;
            emit directionChanged(m_currentDirection);
        }
        return;
    }

    m_state = State::Moving;
    m_currentDirection = nextDirection;
    m_elapsed.restart();
    m_tickTimer.start();
    emit started(m_currentDirection, m_currentMode);
}

void MotionController::finishAtTarget()
{
    m_tickTimer.stop();
    m_currentPosition = m_targetPosition;
    m_state = State::Idle;
    emit positionChanged(m_currentPosition);

    const QPointF percentPosition = currentPercentPosition();
    emit completed(percentPosition.x(), percentPosition.y());
}

void MotionController::interruptWithReason(const QString &reason)
{
    if (m_state == State::Idle) {
        return;
    }

    m_tickTimer.stop();
    m_state = State::Idle;
    emit interrupted(reason);
}
