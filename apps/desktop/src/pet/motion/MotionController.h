#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>

struct MotionConfig
{
    double walkSpeed = 60.0;
    double runSpeed = 120.0;
    double snapDistance = 5.0;
    double petScale = 1.0;
    double petWindowSize = 120.0;
    QStringList availableDirections;
};

class MotionController : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Moving };

    explicit MotionController(QObject *parent = nullptr);

    void configure(const MotionConfig &config);
    void setScreenGeometry(const QRect &screenGeometry);
    void setCurrentPosition(const QPoint &petWindowPosition);

    void moveTo(double x, double y, const QString &mode);
    void moveBy(double dx, double dy, const QString &mode);
    void stop();
    void cancelForDrag();

    State state() const { return m_state; }
    bool isMoving() const { return m_state == State::Moving; }
    bool lastMoveWasClamped() const { return m_lastMoveWasClamped; }
    QPoint currentPosition() const { return m_currentPosition; }
    QPointF currentPercentPositionForResult() const { return currentPercentPosition(); }

signals:
    void started(const QString &movementDirection, const QString &mode);
    void positionChanged(const QPoint &newPosition);
    void directionChanged(const QString &newDirection);
    void completed(double finalX, double finalY);
    void interrupted(const QString &reason);

private:
    void tick();
    QRect reachableGeometry() const;
    QPoint targetForPercent(double x, double y, bool *clamped) const;
    QPoint targetForDeltaPercent(double dx, double dy, bool *clamped) const;
    QPoint clampToReachable(const QPointF &candidate, bool *clamped) const;
    QPointF currentPercentPosition() const;
    QString normalizedMode(const QString &mode) const;
    double speedForMode(const QString &mode) const;
    QString directionForVector(const QPointF &vector) const;
    void beginMove(const QPoint &target, const QString &mode, bool clamped);
    void finishAtTarget();
    void interruptWithReason(const QString &reason);

    MotionConfig m_config;
    QRect m_screenGeometry;
    QPoint m_currentPosition;
    QPoint m_targetPosition;
    State m_state = State::Idle;
    QTimer m_tickTimer;
    QElapsedTimer m_elapsed;
    QString m_currentDirection;
    QString m_currentMode = QStringLiteral("walk");
    bool m_lastMoveWasClamped = false;
};
