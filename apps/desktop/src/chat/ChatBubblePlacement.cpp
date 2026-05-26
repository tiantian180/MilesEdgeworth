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
    const int tailInset = qMin(42, qMax(0, bubbleWidth / 2));

    const QPoint petCenter = pet.center();
    const int minX = available.left() + safeMargin;
    const int minY = available.top() + safeMargin;
    const int maxX = available.right() - bubbleWidth - safeMargin + 1;
    const int maxY = available.bottom() - bubbleHeight - safeMargin + 1;

    const int preferredX = petCenter.x() - bubbleWidth / 2;
    const int aboveY = pet.top() - bubbleHeight - safeMargin;
    const int belowY = pet.bottom() + 1 + safeMargin;
    const bool canPlaceAbove = aboveY >= minY;
    const bool canPlaceBelow = belowY <= maxY;
    const bool placeAbove = canPlaceAbove || !canPlaceBelow;
    const int preferredY = placeAbove ? aboveY : belowY;

    ChatBubblePlacementResult result;
    result.topLeft = QPoint(
        clampCoordinate(preferredX, minX, maxX),
        clampCoordinate(preferredY, minY, maxY)
    );

    result.tailX = clampCoordinate(petCenter.x() - result.topLeft.x(), tailInset, bubbleWidth - tailInset);
    const bool tailOnLeft = result.tailX <= bubbleWidth / 2;
    if (placeAbove) {
        result.pointer = tailOnLeft ? QStringLiteral("bottomLeft") : QStringLiteral("bottomRight");
    } else {
        result.pointer = tailOnLeft ? QStringLiteral("topLeft") : QStringLiteral("topRight");
    }

    return result;
}
