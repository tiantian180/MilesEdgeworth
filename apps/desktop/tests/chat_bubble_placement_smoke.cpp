#include "chat/ChatBubblePlacement.h"

#include <QRect>
#include <QSize>
#include <QString>

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireInsideAvailable(const ChatBubblePlacementResult &result, const QRect &available, const QSize &bubbleSize, int margin)
{
    require(result.topLeft.x() >= available.left() + margin, "bubble should not cross left available margin");
    require(result.topLeft.y() >= available.top() + margin, "bubble should not cross top available margin");
    require(result.topLeft.x() + bubbleSize.width() <= available.right() - margin + 1,
            "bubble should not cross right available margin");
    require(result.topLeft.y() + bubbleSize.height() <= available.bottom() - margin + 1,
            "bubble should not cross bottom available margin");
}
} // namespace

int main()
{
    const QRect screen(0, 0, 1000, 800);
    const QSize bubbleSize(320, 156);
    const int margin = 12;

    {
        const QRect pet(24, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topLeft"), "top-left pet should show bubble below with left tail");
        require(result.topLeft.y() == pet.bottom() + 1 + margin, "top-left pet should keep vertical gap below pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(880, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topRight"), "top-right pet should show bubble below with right tail");
        require(result.topLeft.y() == pet.bottom() + 1 + margin, "top-right pet should keep vertical gap below pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(24, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomLeft"), "bottom-left pet should show bubble above with left tail");
        require(result.topLeft.y() + bubbleSize.height() == pet.top() - margin, "bottom-left pet should keep vertical gap above pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(880, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomRight"), "bottom-right pet should show bubble above with right tail");
        require(result.topLeft.y() + bubbleSize.height() == pet.top() - margin, "bottom-right pet should keep vertical gap above pet");
        require(result.tailX >= 42 && result.tailX <= bubbleSize.width() - 42, "tailX should stay inside safe tail range");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect pet(430, 520, 100, 140);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer.startsWith(QStringLiteral("bottom")), "pet with room above should place bubble above");
        require(result.topLeft.x() == pet.center().x() - bubbleSize.width() / 2,
                "bubble should center horizontally over pet when room exists");
        require(result.tailX == bubbleSize.width() / 2, "centered bubble should use centered tail");
        requireInsideAvailable(result, screen, bubbleSize, margin);
    }

    {
        const QRect tinyScreen(0, 0, 240, 160);
        const QRect pet(90, 70, 80, 80);
        const QSize largeBubble(300, 220);
        const ChatBubblePlacementResult result = placeChatBubble(pet, tinyScreen, largeBubble, margin);
        require(result.topLeft.x() == margin, "oversized bubble should clamp to left margin");
        require(result.topLeft.y() == margin, "oversized bubble should clamp to top margin");
        require(result.tailX >= 42 && result.tailX <= largeBubble.width() - 42, "oversized tailX should still be safe");
    }

    {
        const QRect pet(430, 520, 100, 140);
        const QSize narrowBubble(60, 90);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, narrowBubble, margin);
        require(result.tailX >= 0 && result.tailX <= narrowBubble.width(), "narrow bubble tailX should stay inside bubble");
        require(result.tailX == narrowBubble.width() / 2, "centered narrow bubble should use centered tail");
    }

    {
        const QRect shiftedScreen(-500, -300, 500, 400);
        const QRect pet(-450, -222, 80, 240);
        const ChatBubblePlacementResult result = placeChatBubble(pet, shiftedScreen, bubbleSize, margin);
        requireInsideAvailable(result, shiftedScreen, bubbleSize, margin);
    }

    return 0;
}
