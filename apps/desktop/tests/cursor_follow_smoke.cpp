#include "pet/PetRuntime.h"
#include "pet/interaction/CursorFollowController.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#include <cstdlib>

void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
void advance(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    PetRuntime runtime;
    require(runtime.setActiveSkin("meow-market-leader"), "market skin loads for follow test");
    runtime.setState("idle");
    if (!runtime.autoMovementPreference()) runtime.toggleAutoMovementEnabled();
    CursorFollowController follow(&runtime);
    const int size = qRound(runtime.petWindowSize());
    const QRect screen(0, 0, 1600, 1000);
    QRect pet(600, 250, size, size);
    int positions = 0, toolResults = 0;
    QObject::connect(&runtime, &PetRuntime::motionPositionChanged, [&](const QPoint &p) {
        ++positions;
        pet.moveTopLeft(p);
        require(p.x() >= 0 && p.y() >= 0 && p.x() <= screen.width() - size && p.y() <= screen.height() - size,
                "follow must remain in reachable screen bounds");
    });
    QObject::connect(&runtime, &PetRuntime::motionCompleted, [&] { ++toolResults; });
    QObject::connect(&runtime, &PetRuntime::motionInterrupted, [&] { ++toolResults; });
    follow.update(QPointF(1400, 500), pet, screen, false);
    require(!runtime.pointerMotionActive(), "follow defaults off");
    follow.setEnabled(true);
    follow.update(QPointF(1400, 500), pet, screen, false);
    require(runtime.pointerMotionActive() && runtime.currentState() == "moving", "enable starts local follow");
    require(runtime.currentActionId() == "watch", "single-facing skin follows with its idle action");
    require(runtime.currentLoopMode() == "loop", "walking animation loops while following");
    require(runtime.autoMovementPreference() && !runtime.autoMovementEnabled(), "suppress frame motion without altering preference");
    advance(100);
    require(positions > 0, "follow actually produces movement");
    follow.update(QPointF(100, 500), pet, screen, false);
    require(runtime.currentActionId() == "watch", "retarget preserves the single-facing idle action");
    follow.update(QPointF(100, 500), pet, screen, true);
    require(!runtime.pointerMotionActive() && runtime.currentState() == "idle", "menu/chat/button blocker stops follow");
    const int stoppedPositions = positions;
    advance(100);
    require(positions == stoppedPositions, "paused follow does not continue moving");
    follow.update(pet.center(), pet, screen, false);
    require(!runtime.pointerMotionActive(), "near cursor remains still");
    follow.update(QPointF(-100, 500), pet, screen, false);
    require(!runtime.pointerMotionActive(), "cursor outside permitted screen pauses follow");
    follow.update(QPointF(1400, 500), pet, screen, false);
    runtime.cancelMotionForDrag();
    require(!runtime.pointerMotionActive(), "drag immediately cancels local motion");
    runtime.setState("idle");
    follow.update(QPointF(1400, 500), pet, screen, false);
    runtime.toggleAutoMovementEnabled();
    follow.update(QPointF(1400, 500), pet, screen, false);
    require(!runtime.pointerMotionActive() && !runtime.autoMovementPreference(), "movement prohibition wins and survives stop");
    runtime.toggleAutoMovementEnabled();
    follow.update(QPointF(1400, 500), pet, screen, false);
    runtime.setState("thinking");
    follow.update(QPointF(1400, 500), pet, screen, false);
    require(!runtime.pointerMotionActive() && runtime.currentState() == "thinking", "chat expression interrupts local movement");
    runtime.setState("idle");
    follow.update(QPointF(1400, 500), pet, screen, false);
    runtime.playAction("strike");
    follow.update(QPointF(1400, 500), pet, screen, false);
    require(!runtime.pointerMotionActive() && runtime.currentActionId() == "strike", "follow waits for click interaction animation");
    runtime.setState("idle");
    follow.update(QPointF(1400, 500), pet, screen, false);
    follow.setEnabled(false);
    require(!runtime.pointerMotionActive() && runtime.currentState() == "idle", "disable stops immediately");
    require(toolResults == 0, "local follow must never send agent tool results");
    runtime.requestMotion("moveTo", 0.9, 0.6, "walk");
    follow.setEnabled(true);
    follow.update(QPointF(100, 500), pet, screen, false);
    follow.setEnabled(false);
    require(runtime.currentState() == "moving" && !runtime.pointerMotionActive(), "local switch cannot cancel agent movement");
    runtime.stopMotion();
    require(toolResults == 1, "agent motion result remains functional");
    // Reach a short target and verify natural idle completion without a tool result.
    follow.setEnabled(true);
    const QPointF center(pet.x() + size / 2.0, pet.y() + size / 2.0);
    follow.update(center + QPointF(size / 2.0 + 60, 0), pet, screen, false);
    require(runtime.pointerMotionActive(), "short follow begins outside hysteresis radius");
    advance(1800);
    require(!runtime.pointerMotionActive() && runtime.currentState() == "idle", "arriving returns to idle");
    require(toolResults == 1, "local arrival is not an agent result");
    std::cout << "cursor follow checks passed\n";
}
