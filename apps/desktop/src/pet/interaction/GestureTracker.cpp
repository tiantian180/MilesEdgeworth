#include "pet/interaction/GestureTracker.h"

#include <QtGlobal>

void GestureTracker::startDrag(double globalX)
{
    m_dragShakeTracking = true;
    m_dragShakeX = globalX;
    m_dragShakeDirection = 1;
    m_dragShakeTurns = 0;
    m_dragHoldAnimationCompleted = false;
    m_dragShakeClock.restart();
}

bool GestureTracker::updateDrag(double globalX)
{
    if (!m_dragShakeTracking) {
        return false;
    }

    // 旧版只统计 1 秒窗口内的横向反转次数。超过时间就重新开始计数，
    // 这样慢慢拖动不会误触发“被晃到蹲下”的反应。
    if (m_dragShakeClock.isValid() && m_dragShakeClock.elapsed() > 1000) {
        m_dragShakeClock.restart();
        m_dragShakeTurns = 0;
    }

    const double movement = globalX - m_dragShakeX;
    if (qFuzzyIsNull(movement)) {
        return false;
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
        return true;
    }

    return false;
}

bool GestureTracker::finishDrag()
{
    const bool holdCompleted = m_dragHoldAnimationCompleted;
    reset();
    return holdCompleted;
}

void GestureTracker::markHoldAnimationReachedEnd()
{
    m_dragHoldAnimationCompleted = true;
}

void GestureTracker::reset()
{
    m_dragShakeTracking = false;
    m_dragShakeTurns = 0;
    m_dragHoldAnimationCompleted = false;
}
