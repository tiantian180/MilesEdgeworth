#include "DesktopShellController.h"

#include <QAction>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QRect>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QWindow>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

DesktopShellController::DesktopShellController(QObject *parent)
    : QObject(parent)
{
    createTrayIcon();
}

DesktopShellController::~DesktopShellController()
{
    delete m_trayMenu;
}

bool DesktopShellController::alwaysOnTop() const
{
    return m_alwaysOnTop;
}

void DesktopShellController::setPetWindow(QWindow *window)
{
    if (m_petWindow == window) {
        return;
    }

    m_petWindow = window;

#ifdef Q_OS_MACOS
    // 基础行为只负责“像桌宠窗口”：不因失焦隐藏、透明、禁用普通窗口动画。
    // 是否置顶单独由 applyCurrentLayerMode() 决定，方便菜单动态切换。
    applyMacPetWindowBaseBehavior(m_petWindow);
#endif

    applyCurrentLayerMode();
}

void DesktopShellController::setAlwaysOnTop(bool alwaysOnTop)
{
    if (m_alwaysOnTop == alwaysOnTop) {
        return;
    }

    m_alwaysOnTop = alwaysOnTop;
    applyCurrentLayerMode();
    emit alwaysOnTopChanged();
}

void DesktopShellController::setPetScale(double petScale)
{
    const double safeScale = petScale > 0.0 ? petScale : 2.0;
    if (qFuzzyCompare(m_petScale, safeScale)) {
        return;
    }

    m_petScale = safeScale;

    if (m_petWindow != nullptr) {
        // 旧版切换尺寸后会轻微 move 一下，借 moveEvent 重新夹住边界。
        // v2 直接复用统一移动入口，让尺寸变化后的角色身体仍在屏幕内。
        movePetWindowTo(m_petWindow->position().x(), m_petWindow->position().y());
    }
}

void DesktopShellController::toggleAlwaysOnTop()
{
    setAlwaysOnTop(!m_alwaysOnTop);
}

void DesktopShellController::revealPetWindow()
{
    if (m_petWindow == nullptr) {
        return;
    }

    // 旧版单击托盘图标会 activateWindow。QWindow 侧用 show/raise/
    // requestActivate 组合实现“从托盘找回桌宠”的轻量入口。
    m_petWindow->show();
    m_petWindow->raise();
    m_petWindow->requestActivate();
}

void DesktopShellController::placePetWindowForStartup(double petScale)
{
    if (m_petWindow == nullptr) {
        return;
    }

    // 旧版启动入场不是把整个透明窗口限制在屏幕内，而是先让角色
    // 从左下角略微越界的位置出现，再由公文包入场动画和移动逻辑走进来。
    // 这里直接设置窗口位置，刻意绕过普通移动用的 clampedPetWindowPosition()。
    const QPointF startupPosition = legacyStartupPosition(petScale);
    m_petWindow->setPosition(startupPosition.toPoint());
}

void DesktopShellController::movePetWindowBy(double dx, double dy)
{
    if (m_petWindow == nullptr) {
        return;
    }

    const QPointF currentPosition = QPointF(m_petWindow->position());
    movePetWindowTo(currentPosition.x() + dx, currentPosition.y() + dy);
}

void DesktopShellController::movePetWindowTo(double x, double y)
{
    if (m_petWindow == nullptr) {
        return;
    }

    const QPointF clampedPosition = clampedPetWindowPosition(QPointF(x, y));
    m_petWindow->setPosition(clampedPosition.toPoint());
}

void DesktopShellController::createTrayIcon()
{
    if (m_trayIcon != nullptr) {
        return;
    }

    m_trayMenu = new QMenu();
    m_exitAction = m_trayMenu->addAction("退出");

    // 托盘菜单的退出入口走 QApplication/QCoreApplication，
    // 与旧版 actionExit 一样是“退出整个应用”，不只是隐藏窗口。
    connect(m_exitAction, &QAction::triggered, QCoreApplication::instance(), &QCoreApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icon/favicon-bar.ico"));
    m_trayIcon->setToolTip("MilesEdgeworth");
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            revealPetWindow();
        }
    });
}

void DesktopShellController::applyCurrentLayerMode()
{
    if (m_petWindow == nullptr) {
        return;
    }

#ifdef Q_OS_MACOS
    setMacPetWindowAlwaysOnTop(m_petWindow, m_alwaysOnTop);
#else
    // 其他平台先使用 Qt 公开窗口标志兜底。
    // 这里暂时不再强制 Qt::Tool：macOS 已经由原生层处理辅助应用行为；
    // Windows/Linux 后续需要结合托盘和任务栏策略单独验证。
    m_petWindow->setFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
    m_petWindow->show();
#endif
}

QPointF DesktopShellController::legacyStartupPosition(double petScale) const
{
    QScreen *targetScreen = QGuiApplication::primaryScreen();
    if (targetScreen == nullptr) {
        return {};
    }

    const QRect availableGeometry = targetScreen->availableGeometry();
    const double safeScale = petScale > 0.0 ? petScale : 2.0;

    return QPointF(
        availableGeometry.x() - 45.0 * safeScale,
        availableGeometry.y() + availableGeometry.height() - 90.0 * safeScale
    );
}

QPointF DesktopShellController::clampedPetWindowPosition(const QPointF &candidatePosition) const
{
    if (m_petWindow == nullptr) {
        return candidatePosition;
    }

    // 旧版不是按透明窗口外壳限制边界，而是取角色身体附近的四个点：
    // (36,10)、(63,10)、(36,90)、(63,90) * scale。
    // 这样透明留白可以略微越过屏幕边缘，但 Miles 的身体仍会留在屏幕内。
    const QPointF legacyBodyCenter(
        candidatePosition.x() + 50.0 * m_petScale,
        candidatePosition.y() + 50.0 * m_petScale
    );

    QScreen *targetScreen = QGuiApplication::screenAt(legacyBodyCenter.toPoint());
    if (targetScreen == nullptr) {
        targetScreen = m_petWindow->screen();
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (targetScreen == nullptr) {
        return candidatePosition;
    }

    const QRect screenGeometry = targetScreen->geometry();
    const double minX = screenGeometry.x() - 36.0 * m_petScale;
    const double minY = screenGeometry.y() - 10.0 * m_petScale;
    const double maxX = screenGeometry.x() + screenGeometry.width() - 63.0 * m_petScale;
    const double maxY = screenGeometry.y() + screenGeometry.height() - 90.0 * m_petScale;

    const double clampedX = (maxX >= minX) ? qBound(minX, candidatePosition.x(), maxX) : minX;
    const double clampedY = (maxY >= minY) ? qBound(minY, candidatePosition.y(), maxY) : minY;

    return QPointF(clampedX, clampedY);
}
