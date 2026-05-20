#include "pet/interaction/GestureTracker.h"

#include <QtMath>
#include <QtGlobal>

namespace {
constexpr double kShakeMoveStepPx = 4.0;
constexpr int kShakeWindowMs = 1000;
constexpr int kShakeTurnThreshold = 4;
} // namespace

void GestureTracker::startDrag(double globalX)
{
    m_dragShakeTracking = true;
    m_dragShakeX = globalX;
    m_dragShakeDirection = 0;
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
    if (m_dragShakeClock.isValid() && m_dragShakeClock.elapsed() > kShakeWindowMs) {
        m_dragShakeClock.restart();
        m_dragShakeTurns = 0;
        m_dragShakeDirection = 0;
        m_dragShakeX = globalX;
    }

    const double movement = globalX - m_dragShakeX;
    if (qAbs(movement) < kShakeMoveStepPx) {
        return false;
    }

    const int nextDirection = movement > 0 ? 1 : -1;
    // v1 的实现以“上一次反转点”为基准，手工快速晃动时需要跨过旧基准点才会计数。
    // v2 这里改为按连续采样的移动方向计数：只要左右方向真实发生切换，就记录一次。
    // kShakeMoveStepPx 用来过滤像素级抖动，避免普通拖拽被误判为晃动。
    if (m_dragShakeDirection != 0 && nextDirection != m_dragShakeDirection) {
        ++m_dragShakeTurns;
    }

    m_dragShakeDirection = nextDirection;
    m_dragShakeX = globalX;

    if (m_dragShakeTurns >= kShakeTurnThreshold) {
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
    m_dragShakeDirection = 0;
    m_dragHoldAnimationCompleted = false;
}
