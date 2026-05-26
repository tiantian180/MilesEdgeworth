#include "pet/surface/PetVisibleBounds.h"

#include <QColor>
#include <QImage>
#include <QRect>

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    {
        QImage image(6, 5, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        image.setPixelColor(1, 2, QColor(10, 20, 30, 255));
        image.setPixelColor(2, 2, QColor(10, 20, 30, 200));
        image.setPixelColor(3, 3, QColor(10, 20, 30, 120));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(bounds == QRect(1, 2, 3, 2), "alpha bounds should cover opaque pixels only");
    }

    {
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        image.setPixelColor(2, 2, QColor(10, 20, 30, 7));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(!bounds.isValid(), "pixels below threshold should not count as visible");
    }

    {
        QImage image(3, 2, QImage::Format_RGB32);
        image.fill(QColor(40, 50, 60));

        const QRect bounds = visibleBoundsFromImage(image, 8);
        require(bounds == QRect(0, 0, 3, 2), "opaque RGB images should be fully visible");
    }

    {
        const QRect bounds = visibleBoundsFromImage(QImage(), 8);
        require(!bounds.isValid(), "null image should produce invalid bounds");
    }

    return 0;
}
