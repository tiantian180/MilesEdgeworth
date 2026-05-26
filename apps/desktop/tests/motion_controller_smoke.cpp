#include "pet/motion/MotionController.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPoint>
#include <QRect>
#include <QThread>

#include <cmath>
#include <functional>
#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool waitFor(const std::function<bool()> &predicate, int timeoutMs = 1200)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

MotionConfig testConfig()
{
    MotionConfig config;
    config.walkSpeed = 300.0;
    config.runSpeed = 600.0;
    config.snapDistance = 4.0;
    config.petScale = 1.0;
    config.petWindowSize = 20.0;
    config.availableDirections = {
        QStringLiteral("east"),
        QStringLiteral("west"),
        QStringLiteral("north"),
        QStringLiteral("south"),
        QStringLiteral("northEast"),
        QStringLiteral("northWest"),
        QStringLiteral("southEast"),
        QStringLiteral("southWest"),
    };
    return config;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    MotionController controller;
    controller.configure(testConfig());
    controller.setScreenGeometry(QRect(0, 0, 120, 100));
    controller.setCurrentPosition(QPoint(0, 0));

    QString startedDirection;
    QString startedMode;
    QPoint lastPosition;
    bool completed = false;
    double finalX = -1.0;
    double finalY = -1.0;
    int positionChangedCount = 0;

    QObject::connect(&controller, &MotionController::started, &controller,
                     [&](const QString &direction, const QString &mode) {
                         startedDirection = direction;
                         startedMode = mode;
                     });
    QObject::connect(&controller, &MotionController::positionChanged, &controller,
                     [&](const QPoint &position) {
                         lastPosition = position;
                         ++positionChangedCount;
                     });
    QObject::connect(&controller, &MotionController::completed, &controller,
                     [&](double x, double y) {
                         completed = true;
                         finalX = x;
                         finalY = y;
                     });

    controller.moveTo(1.0, 1.0, QStringLiteral("run"));
    require(startedDirection == QStringLiteral("southEast"), "moveTo bottom-right should start southEast");
    require(startedMode == QStringLiteral("run"), "run mode should be preserved");
    require(controller.isMoving(), "controller should be moving after moveTo");
    require(waitFor([&]() { return completed; }), "moveTo should complete");
    require(positionChangedCount > 0, "moveTo should emit position changes");
    require(lastPosition == QPoint(100, 80), "reachable bottom-right should subtract pet window size");
    require(std::abs(finalX - 1.0) < 0.001, "final x percent should be 1");
    require(std::abs(finalY - 1.0) < 0.001, "final y percent should be 1");
    require(!controller.isMoving(), "controller should be idle after completion");

    completed = false;
    positionChangedCount = 0;
    controller.setCurrentPosition(QPoint(50, 40));
    controller.moveBy(1.0, -1.0, QStringLiteral("walk"));
    require(startedDirection == QStringLiteral("northEast"), "moveBy up-right should start northEast");
    require(startedMode == QStringLiteral("walk"), "walk mode should be preserved");
    require(waitFor([&]() { return completed; }), "moveBy should complete");
    require(lastPosition == QPoint(100, 0), "moveBy should clamp to reachable screen edge");
    require(controller.lastMoveWasClamped(), "clamped move should remember clamp note");

    bool interrupted = false;
    QString interruptedReason;
    QObject::connect(&controller, &MotionController::interrupted, &controller,
                     [&](const QString &reason) {
                         interrupted = true;
                         interruptedReason = reason;
                     });
    controller.setCurrentPosition(QPoint(0, 0));
    controller.moveTo(1.0, 0.0, QStringLiteral("walk"));
    require(controller.isMoving(), "controller should move before drag cancel");
    controller.cancelForDrag();
    require(interrupted, "drag cancel should emit interrupted");
    require(interruptedReason == QStringLiteral("drag"), "drag cancel reason should be drag");
    require(!controller.isMoving(), "controller should be idle after drag cancel");

    MotionConfig scaledConfig = testConfig();
    scaledConfig.walkSpeed = 1.0;
    scaledConfig.snapDistance = 5.0;
    scaledConfig.petScale = 2.0;
    scaledConfig.petWindowSize = 0.0;

    MotionController scaledController;
    scaledController.configure(scaledConfig);
    scaledController.setScreenGeometry(QRect(0, 0, 100, 100));
    scaledController.setCurrentPosition(QPoint(0, 0));

    bool scaledCompleted = false;
    QObject::connect(&scaledController, &MotionController::completed, &scaledController,
                     [&](double, double) {
                         scaledCompleted = true;
                     });
    scaledController.moveTo(0.09, 0.0, QStringLiteral("walk"));
    require(waitFor([&]() { return scaledCompleted; }, 200), "snap distance should scale with petScale");

    MotionConfig partialDirectionConfig = testConfig();
    partialDirectionConfig.availableDirections = {
        QStringLiteral("south"),
        QStringLiteral("north"),
    };

    MotionController partialDirectionController;
    partialDirectionController.configure(partialDirectionConfig);
    partialDirectionController.setScreenGeometry(QRect(0, 0, 200, 200));
    partialDirectionController.setCurrentPosition(QPoint(50, 50));

    QString partialStartedDirection;
    QObject::connect(&partialDirectionController, &MotionController::started, &partialDirectionController,
                     [&](const QString &direction, const QString &) {
                         partialStartedDirection = direction;
                     });
    partialDirectionController.moveBy(1.0, -0.1, QStringLiteral("walk"));
    require(partialStartedDirection == QStringLiteral("north"),
            "partial direction skin should choose nearest available canonical direction");
    partialDirectionController.stop();

    MotionController reclampController;
    reclampController.configure(testConfig());
    reclampController.setScreenGeometry(QRect(0, 0, 120, 100));
    reclampController.setCurrentPosition(QPoint(0, 0));

    bool reclampCompleted = false;
    double reclampFinalX = -1.0;
    QObject::connect(&reclampController, &MotionController::completed, &reclampController,
                     [&](double x, double) {
                         reclampCompleted = true;
                         reclampFinalX = x;
                     });
    reclampController.moveTo(1.0, 0.0, QStringLiteral("walk"));
    reclampController.setScreenGeometry(QRect(0, 0, 70, 100));
    require(reclampController.lastMoveWasClamped(), "moving target should remember reclamp after geometry shrink");
    require(waitFor([&]() { return reclampCompleted; }), "reclamped move should complete");
    require(reclampController.currentPosition() == QPoint(50, 0), "moving target should reclamp to new reachable edge");
    require(std::abs(reclampFinalX - 1.0) < 0.001, "reclamped completion should report new reachable edge percent");

    return 0;
}
