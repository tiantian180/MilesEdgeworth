#include "DesktopShellController.h"
#include "pet/events/PetEventBridge.h"
#include "pet/PetRuntime.h"
#include "pet/surface/PetSurfaceWindow.h"

#include <QApplication>
#include <QTimer>
#include <QWindow>

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
    PetEventBridge petEventBridge(&petRuntime);
    PetEventBridgeForeign::s_instance = &petEventBridge;

    shellController.setPetScale(petRuntime.petScale());
    QObject::connect(&petRuntime, &PetRuntime::petScaleChanged, &shellController, [&shellController, &petRuntime]() {
        shellController.setPetScale(petRuntime.petScale());
    });

    PetSurfaceWindow petSurfaceWindow(&petRuntime, &petEventBridge, &shellController);
    petSurfaceWindow.show();
    petSurfaceWindow.winId();

    // QWidget 需要先创建 native handle，macOS 原生层才能拿到 NSWindow。
    QTimer::singleShot(0, &petSurfaceWindow, [&petSurfaceWindow, &shellController, &petRuntime]() {
        shellController.setPetWindow(petSurfaceWindow.windowHandle());
        shellController.placePetWindowForStartup(petRuntime.petScale());
    });

    return app.exec();
}
