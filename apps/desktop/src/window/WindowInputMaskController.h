#pragma once

#include <QRegion>
#include <QUrl>

class QWindow;

class WindowInputMaskController
{
public:
    static void applyMask(
        QWindow *window,
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold = 8
    );

    static void clearMask(QWindow *window);

private:
    static QRegion regionFromImage(
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold
    );
};
