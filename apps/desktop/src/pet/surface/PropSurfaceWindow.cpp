#include "pet/surface/PropSurfaceWindow.h"

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QRegion>
#include <QWindow>
#include <QtGlobal>

namespace {
constexpr int kAlphaThreshold = 8;
} // namespace

PropSurfaceWindow::PropSurfaceWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setWindowFlags(Qt::FramelessWindowHint
        | Qt::NoDropShadowWindowHint
        | Qt::Tool);
    setCursor(Qt::PointingHandCursor);
}

void PropSurfaceWindow::setAlwaysOnTop(bool alwaysOnTop)
{
    m_alwaysOnTop = alwaysOnTop;
    applyPlatformWindowBehavior();
}

void PropSurfaceWindow::showPixmap(
    const QString &path,
    const QSize &windowSize,
    const QSize &visualSize,
    const QPoint &globalPosition
)
{
    m_pixmap = QPixmap(path);
    m_visualSize = visualSize;
    setFixedSize(windowSize);
    move(globalPosition);
    applyPixmapMask();
    applyPlatformWindowBehavior();
    show();
    raise();
    update();
}

void PropSurfaceWindow::setClickedCallback(std::function<void()> callback)
{
    m_clickedCallback = std::move(callback);
}

void PropSurfaceWindow::paintEvent(QPaintEvent *)
{
    if (m_pixmap.isNull()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRect target(
        (width() - m_visualSize.width()) / 2,
        (height() - m_visualSize.height()) / 2,
        m_visualSize.width(),
        m_visualSize.height()
    );
    painter.drawPixmap(target, m_pixmap);
}

void PropSurfaceWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_clickedCallback) {
        m_clickedCallback();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PropSurfaceWindow::applyPlatformWindowBehavior()
{
#ifdef Q_OS_MACOS
    // QWidget 只有拿到 native handle 后，AppKit 层才能找到 NSWindow。
    // Prop 是桌宠的临时视觉对象，窗口策略必须跟桌宠本体一致：
    // 应用失焦不隐藏，且“始终置顶”开关改变时同步调整层级。
    winId();
    QWindow *window = windowHandle();
    if (window == nullptr) {
        return;
    }

    applyMacPetWindowBaseBehavior(window);
    setMacPetWindowAlwaysOnTop(window, m_alwaysOnTop);
#else
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
    if (wasVisible) {
        show();
        raise();
    }
#endif
}

void PropSurfaceWindow::applyPixmapMask()
{
    if (m_pixmap.isNull()) {
        clearMask();
        return;
    }

    const QImage image = m_pixmap
        .toImage()
        .scaled(m_visualSize, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_ARGB32);
    QRegion region;
    const int offsetX = (width() - image.width()) / 2;
    const int offsetY = (height() - image.height()) / 2;

    for (int y = 0; y < image.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x < image.width(); ++x) {
            const bool opaque = qAlpha(image.pixel(x, y)) > kAlphaThreshold;
            if (opaque && runStart < 0) {
                runStart = x;
            }

            const bool atLastPixel = (x == image.width() - 1);
            if ((!opaque || atLastPixel) && runStart >= 0) {
                const int runEnd = (opaque && atLastPixel) ? x + 1 : x;
                region += QRect(offsetX + runStart, offsetY + y, runEnd - runStart, 1);
                runStart = -1;
            }
        }
    }

    if (region.isEmpty()) {
        clearMask();
        return;
    }

    setMask(region);
}
