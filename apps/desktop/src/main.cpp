#include "DesktopShellController.h"
#include "pet/PetRuntime.h"

#include <QApplication>
#include <QQmlApplicationEngine>
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
    // Qt.labs.platform 的原生菜单在部分平台需要 Qt Widgets fallback。
    // 因此桌面壳层使用 QApplication，而不是更轻的 QGuiApplication。
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    DesktopShellController shellController;
    DesktopShellControllerForeign::s_instance = &shellController;

    PetRuntime petRuntime;
    PetRuntimeForeign::s_instance = &petRuntime;

    QQmlApplicationEngine engine;
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
