#include "chat/ChatBubblePlacement.h"

#include <QRect>
#include <QSize>

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
    const QRect screen(0, 0, 1000, 800);
    const QSize bubbleSize(280, 120);
    const int margin = 12;

    {
        const QRect pet(24, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topLeft"),
                "top-left pet should use a top-left pointer");
        require(result.topLeft.x() >= pet.right(),
                "top-left pet should place the bubble to the pet's right");
        require(result.topLeft.y() >= pet.bottom(),
                "top-left pet should place the bubble below the pet");
        require(result.topLeft.x() == pet.right() + 1 + margin,
                "top-left pet should keep an exact horizontal margin");
        require(result.topLeft.y() == pet.bottom() + 1 + margin,
                "top-left pet should keep an exact vertical margin");
    }

    {
        const QRect pet(880, 32, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("topRight"),
                "top-right pet should use a top-right pointer");
        require(result.topLeft.x() + bubbleSize.width() <= pet.left(),
                "top-right pet should place the bubble to the pet's left");
        require(result.topLeft.y() >= pet.bottom(),
                "top-right pet should place the bubble below the pet");
        require(result.topLeft.x() == pet.left() - bubbleSize.width() - margin,
                "top-right pet should keep an exact horizontal margin");
        require(result.topLeft.y() == pet.bottom() + 1 + margin,
                "top-right pet should keep an exact vertical margin");
    }

    {
        const QRect pet(24, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomLeft"),
                "bottom-left pet should use a bottom-left pointer");
        require(result.topLeft.x() >= pet.right(),
                "bottom-left pet should place the bubble to the pet's right");
        require(result.topLeft.y() + bubbleSize.height() <= pet.top(),
                "bottom-left pet should place the bubble above the pet");
        require(result.topLeft.x() == pet.right() + 1 + margin,
                "bottom-left pet should keep an exact horizontal margin");
        require(result.topLeft.y() == pet.top() - bubbleSize.height() - margin,
                "bottom-left pet should keep an exact vertical margin");
    }

    {
        const QRect pet(880, 636, 96, 132);
        const ChatBubblePlacementResult result = placeChatBubble(pet, screen, bubbleSize, margin);
        require(result.pointer == QStringLiteral("bottomRight"),
                "bottom-right pet should use a bottom-right pointer");
        require(result.topLeft.x() + bubbleSize.width() <= pet.left(),
                "bottom-right pet should place the bubble to the pet's left");
        require(result.topLeft.y() + bubbleSize.height() <= pet.top(),
                "bottom-right pet should place the bubble above the pet");
        require(result.topLeft.x() == pet.left() - bubbleSize.width() - margin,
                "bottom-right pet should keep an exact horizontal margin");
        require(result.topLeft.y() == pet.top() - bubbleSize.height() - margin,
                "bottom-right pet should keep an exact vertical margin");
    }

    {
        const QRect tinyScreen(0, 0, 240, 160);
        const QRect pet(90, 70, 80, 80);
        const QSize largeBubble(300, 220);
        const ChatBubblePlacementResult result = placeChatBubble(pet, tinyScreen, largeBubble, margin);
        require(result.topLeft.x() == margin,
                "oversized bubble should clamp to the available left margin");
        require(result.topLeft.y() == margin,
                "oversized bubble should clamp to the available top margin");
    }

    {
        const QRect narrowScreen(0, 0, 500, 400);
        const QRect pet(170, 340, 120, 40);
        const ChatBubblePlacementResult result = placeChatBubble(pet, narrowScreen, bubbleSize, margin);
        require(result.topLeft.x() == narrowScreen.right() - bubbleSize.width() - margin + 1,
                "normal bubble should clamp to the available right margin");
    }

    {
        const QRect shiftedScreen(-500, -300, 500, 400);
        const QRect pet(-450, -222, 80, 240);
        const ChatBubblePlacementResult result = placeChatBubble(pet, shiftedScreen, bubbleSize, margin);
        require(result.topLeft.y() == shiftedScreen.bottom() - bubbleSize.height() - margin + 1,
                "normal bubble should clamp to the available bottom margin with a shifted screen");
    }

    return 0;
}
