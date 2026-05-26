#include "pet/surface/PetVisibleBounds.h"

#include <QColor>
#include <QtGlobal>

QRect visibleBoundsFromImage(const QImage &sourceImage, int alphaThreshold)
{
    if (sourceImage.isNull()) {
        return {};
    }

    const QImage image = sourceImage.format() == QImage::Format_ARGB32
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return {};
    }

    const int threshold = qBound(0, alphaThreshold, 255);
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(row[x]) <= threshold) {
                continue;
            }

            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return {};
    }

    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}
