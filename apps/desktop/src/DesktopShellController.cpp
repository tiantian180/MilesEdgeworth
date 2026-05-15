#include "DesktopShellController.h"

#include <QWindow>

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

void DesktopShellController::applyCurrentLayerMode()
{
    if (m_petWindow == nullptr) {
        return;
    }

#ifdef Q_OS_MACOS
    setMacPetWindowAlwaysOnTop(m_petWindow, m_alwaysOnTop);
#else
    // 其他平台先使用 Qt 公开窗口标志兜底。
    // 修改窗口 flag 后主动 show 一次，避免部分窗口系统重新创建 native window 时短暂隐藏。
    m_petWindow->setFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
    m_petWindow->show();
#endif
}
