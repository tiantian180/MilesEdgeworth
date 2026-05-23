#include "DesktopShellController.h"
#include "chat/ChatController.h"
#include "logging/MilesLogHandler.h"
#include "pet/events/PetEventBridge.h"
#include "pet/interaction/CustomInteractionRegistry.h"
#include "pet/PetRuntime.h"
#include "pet/surface/PetSurfaceWindow.h"
#include "settings/SettingsController.h"
#include "settings/SettingsService.h"
#include "skins/miles-edgeworth/MilesEdgeworthInteractions.h"

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QWindow>

#include <memory>

int main(int argc, char *argv[])
{
    MilesLogHandler::install();

    // Qt.labs.platform 的原生菜单在部分平台需要 Qt Widgets fallback。
    // 因此桌面壳层使用 QApplication，而不是更轻的 QGuiApplication。
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("tian"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("dev.tian.MilesEdgeworth"));
    QCoreApplication::setApplicationName(QStringLiteral("MilesEdgeworth"));
    app.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");
#ifdef Q_OS_MACOS
    setMacApplicationDockVisible(false);
#endif

    DesktopShellController shellController;
    DesktopShellControllerForeign::s_instance = &shellController;

    PetRuntime petRuntime;
    auto registerCurrentSkinInteractions = [&petRuntime]() {
        CustomInteractionRegistry::reset();
        CustomInteractionRegistry::registerBuiltins(petRuntime.manifest());
        registerMilesEdgeworthInteractions(petRuntime.manifest());
    };
    registerCurrentSkinInteractions();
    QObject::connect(&petRuntime, &PetRuntime::skinManifestReloaded, &app, registerCurrentSkinInteractions);
    PetRuntimeForeign::s_instance = &petRuntime;
    PetEventBridge petEventBridge(&petRuntime);
    PetEventBridgeForeign::s_instance = &petEventBridge;

    SettingsService settingsService;
    SettingsController settingsController(&settingsService, &petRuntime);
    SettingsControllerForeign::s_instance = &settingsController;

    ChatController chatController(&petRuntime, &settingsService);
    ChatControllerForeign::s_instance = &chatController;
    QObject::connect(&settingsController, &SettingsController::saved, &chatController, &ChatController::handleSettingsSaved);

    QQmlApplicationEngine chatEngine;
    chatEngine.loadFromModule("MilesEdgeworth", "ChatWindow");
    chatEngine.loadFromModule("MilesEdgeworth", "SettingsWindow");
    if (chatEngine.rootObjects().size() < 2) {
        return 1;
    }
    chatController.startSidecar();

    shellController.setPetScale(petRuntime.petScale());
    QObject::connect(&petRuntime, &PetRuntime::petScaleChanged, &shellController, [&shellController, &petRuntime]() {
        shellController.setPetScale(petRuntime.petScale());
    });

    PetSurfaceWindow petSurfaceWindow(&petRuntime, &petEventBridge, &shellController, &chatController);
    petSurfaceWindow.show();
    petSurfaceWindow.winId();

    // QWidget 需要先创建 native handle，macOS 原生层才能拿到 NSWindow。
    QTimer::singleShot(0, &petSurfaceWindow, [&petSurfaceWindow, &shellController, &petRuntime]() {
        shellController.setPetWindow(petSurfaceWindow.windowHandle());
        shellController.placePetWindowForStartup(petRuntime.petScale());
    });

    return app.exec();
}
