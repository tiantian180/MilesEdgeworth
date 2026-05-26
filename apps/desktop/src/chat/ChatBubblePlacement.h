#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

struct ChatBubblePlacementResult
{
    QPoint topLeft;
    QString pointer;
};

ChatBubblePlacementResult placeChatBubble(
    const QRect &petGeometry,
    const QRect &availableGeometry,
    const QSize &bubbleSize,
    int margin
);
