#pragma once

#include <QElapsedTimer>

// GestureTracker 只负责把连续鼠标输入识别成“手势事实”。
//
// 它不知道任何 action id、recipe id 或皮肤名字。比如 Miles 的“被晃到蹲下”
// 由 InteractionPipeline + manifest behaviorRules 决定，Tracker 只回答：
// 1 秒内横向反向移动次数是否达到旧版阈值，hold 动画是否已经播到末帧。
class GestureTracker
{
public:
    void startDrag(double globalX);
    bool updateDrag(double globalX);
    bool finishDrag();
    void markHoldAnimationReachedEnd();
    void reset();

private:
    QElapsedTimer m_dragShakeClock;
    double m_dragShakeX = 0;
    int m_dragShakeDirection = 1;
    int m_dragShakeTurns = 0;
    bool m_dragShakeTracking = false;
    bool m_dragHoldAnimationCompleted = false;
};
