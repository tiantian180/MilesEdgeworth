#include "DesktopShellController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QWindow>

namespace {
void attachPetWindowToShellController(QQmlApplicationEngine &engine, DesktopShellController &shellController)
{
    if (engine.rootObjects().isEmpty()) {
        return;
    }

    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }

    shellController.setPetWindow(window);
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    DesktopShellController shellController;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("desktopShell", &shellController);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("MilesEdgeworth", "PetWindow");

    // 等 QML Window 创建完 native handle 后，再追加平台级桌宠窗口行为。
    QTimer::singleShot(0, &engine, [&engine, &shellController]() {
        attachPetWindowToShellController(engine, shellController);
    });

    return app.exec();
}
