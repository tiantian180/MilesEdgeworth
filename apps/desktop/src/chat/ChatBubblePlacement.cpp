#include "chat/ChatBubblePlacement.h"

#include <QtGlobal>

namespace {
int clampCoordinate(int value, int minimum, int maximum)
{
    return maximum >= minimum ? qBound(minimum, value, maximum) : minimum;
}

int safeDimension(int value)
{
    return qMax(1, value);
}
} // namespace

ChatBubblePlacementResult placeChatBubble(
    const QRect &petGeometry,
    const QRect &availableGeometry,
    const QSize &bubbleSize,
    int margin
)
{
    const QRect available = availableGeometry.isValid()
        ? availableGeometry
        : QRect(0, 0, safeDimension(bubbleSize.width()), safeDimension(bubbleSize.height()));
    const QRect pet = petGeometry.isValid()
        ? petGeometry
        : QRect(available.center().x(), available.center().y(), 1, 1);
    const int safeMargin = qMax(0, margin);
    const int bubbleWidth = safeDimension(bubbleSize.width());
    const int bubbleHeight = safeDimension(bubbleSize.height());

    const QPoint petCenter = pet.center();
    const QPoint screenCenter = available.center();
    const bool petOnLeft = petCenter.x() < screenCenter.x();
    const bool petOnTop = petCenter.y() < screenCenter.y();

    const int preferredX = petOnLeft
        ? pet.right() + 1 + safeMargin
        : pet.left() - bubbleWidth - safeMargin;
    const int preferredY = petOnTop
        ? pet.bottom() + 1 + safeMargin
        : pet.top() - bubbleHeight - safeMargin;

    const int minX = available.left() + safeMargin;
    const int minY = available.top() + safeMargin;
    const int maxX = available.right() - bubbleWidth - safeMargin + 1;
    const int maxY = available.bottom() - bubbleHeight - safeMargin + 1;

    ChatBubblePlacementResult result;
    result.topLeft = QPoint(
        clampCoordinate(preferredX, minX, maxX),
        clampCoordinate(preferredY, minY, maxY)
    );

    if (petOnTop && petOnLeft) {
        result.pointer = QStringLiteral("topLeft");
    } else if (petOnTop) {
        result.pointer = QStringLiteral("topRight");
    } else if (petOnLeft) {
        result.pointer = QStringLiteral("bottomLeft");
    } else {
        result.pointer = QStringLiteral("bottomRight");
    }

    return result;
}
