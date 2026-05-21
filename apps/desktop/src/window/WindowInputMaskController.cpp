#include "window/WindowInputMaskController.h"

#include <QImage>
#include <QImageReader>
#include <QRect>
#include <QWindow>
#include <QtGlobal>

namespace {
QString imagePathFromUrl(const QUrl &url)
{
    if (url.scheme() == "qrc") {
        return QStringLiteral(":") + url.path();
    }
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return url.toString();
}
} // namespace

void WindowInputMaskController::applyMask(
    QWindow *window,
    const QUrl &animationUrl,
    double imageSize,
    double windowSize,
    int alphaThreshold
)
{
    if (window == nullptr || animationUrl.isEmpty()) {
        return;
    }

    const QRegion region = regionFromImage(animationUrl, imageSize, windowSize, alphaThreshold);
    if (region.isEmpty()) {
        clearMask(window);
        return;
    }

    // QWindow::setMask 会把窗口输入区域限制到 region 内。
    // 对桌宠来说，这能让透明留白区域尽量把鼠标事件交还给底下窗口。
    window->setMask(region);
}

void WindowInputMaskController::clearMask(QWindow *window)
{
    if (window == nullptr) {
        return;
    }

    window->setMask(QRegion());
}

QRegion WindowInputMaskController::regionFromImage(
    const QUrl &animationUrl,
    double imageSize,
    double windowSize,
    int alphaThreshold
)
{
    QImageReader reader(imagePathFromUrl(animationUrl));
    reader.setAutoTransform(true);
    const QImage image = reader.read().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    const double safeImageSize = imageSize > 0.0 ? imageSize : windowSize;
    const double safeWindowSize = windowSize > 0.0 ? windowSize : safeImageSize;
    const double scaleX = safeImageSize / image.width();
    const double scaleY = safeImageSize / image.height();
    const double offsetX = (safeWindowSize - safeImageSize) / 2.0;
    const double offsetY = (safeWindowSize - safeImageSize) / 2.0;

    QRegion region;
    for (int y = 0; y < image.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x < image.width(); ++x) {
            const bool opaque = qAlpha(image.pixel(x, y)) > alphaThreshold;
            if (opaque && runStart < 0) {
                runStart = x;
            }

            const bool atLastPixel = (x == image.width() - 1);
            if ((!opaque || atLastPixel) && runStart >= 0) {
                const int runEnd = (opaque && atLastPixel) ? x + 1 : x;
                const QRect rect(
                    qRound(offsetX + runStart * scaleX),
                    qRound(offsetY + y * scaleY),
                    qMax(1, qRound((runEnd - runStart) * scaleX)),
                    qMax(1, qRound(scaleY))
                );
                region += rect;
                runStart = -1;
            }
        }
    }

    return region;
}
