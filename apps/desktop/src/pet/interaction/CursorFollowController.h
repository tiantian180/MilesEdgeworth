#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QPointF>
#include <QRect>

class PetRuntime;

// Local interaction policy. The surface supplies desktop coordinates and input
// blockers; PetRuntime/MotionController remain the only target-motion executor.
class CursorFollowController : public QObject
{
    Q_OBJECT
public:
    explicit CursorFollowController(PetRuntime *runtime, QObject *parent = nullptr);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    void update(const QPointF &cursor, const QRect &pet, const QRect &screen, bool blocked);
    void pause();
signals:
    void enabledChanged();
    void arrived();
private:
    PetRuntime *m_runtime;
    bool m_enabled = false;
    bool m_chasing = false;
    QElapsedTimer m_lastArrival;
};
