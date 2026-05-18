#include "DesktopShellController.h"

#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QWindow>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

DesktopShellController::DesktopShellController(QObject *parent)
    : QObject(parent)
{
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

void DesktopShellController::toggleAlwaysOnTop()
{
    setAlwaysOnTop(!m_alwaysOnTop);
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

QPointF DesktopShellController::clampedPetWindowPosition(const QPointF &candidatePosition) const
{
    if (m_petWindow == nullptr) {
        return candidatePosition;
    }

    // 旧版在 moveEvent 里限制窗口，避免自动走路/跑步把桌宠带出屏幕。
    // v2 先按窗口外壳做保守夹取；后续如果引入 alpha mask / body bounds，
    // 可以把这里的 margin 改成由皮肤 manifest 提供。
    const QSize windowSize = m_petWindow->size();
    const QPointF windowCenter(
        candidatePosition.x() + windowSize.width() / 2.0,
        candidatePosition.y() + windowSize.height() / 2.0
    );

    QScreen *targetScreen = QGuiApplication::screenAt(windowCenter.toPoint());
    if (targetScreen == nullptr) {
        targetScreen = m_petWindow->screen();
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (targetScreen == nullptr) {
        return candidatePosition;
    }

    const QRect availableGeometry = targetScreen->availableGeometry();
    const double minX = availableGeometry.x();
    const double minY = availableGeometry.y();
    const double maxX = availableGeometry.x() + availableGeometry.width() - windowSize.width();
    const double maxY = availableGeometry.y() + availableGeometry.height() - windowSize.height();

    const double clampedX = (maxX >= minX) ? qBound(minX, candidatePosition.x(), maxX) : minX;
    const double clampedY = (maxY >= minY) ? qBound(minY, candidatePosition.y(), maxY) : minY;

    return QPointF(clampedX, clampedY);
}
