#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

namespace {
void applyPlatformPetWindowBehavior(QQmlApplicationEngine &engine)
{
    if (engine.rootObjects().isEmpty()) {
        return;
    }

    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }

#ifdef Q_OS_MACOS
    applyMacPetWindowBehavior(window);
#else
    window->setFlag(Qt::WindowStaysOnTopHint, true);
#endif
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MilesEdgeworth", "PetWindow");

    // 等 QML Window 创建完 native handle 后，再追加平台级桌宠窗口行为。
    QTimer::singleShot(0, &engine, [&engine]() {
        applyPlatformPetWindowBehavior(engine);
    });

    return app.exec();
}
