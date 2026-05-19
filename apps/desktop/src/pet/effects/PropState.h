#pragma once

#include <QPointF>
#include <QString>
#include <QUrl>

// PropState 是当前正在显示的主体外临时视觉对象。
//
// Runtime 和 QML 只关心这份状态，不需要知道它来自哪个皮肤玩法。
struct PropState
{
    QString id;
    QUrl imageUrl;
    QPointF startOffset;
    QPointF endOffset;
    double width = 0;
    double height = 0;
    double visualWidth = 0;
    double visualHeight = 0;
    int durationMs = 0;
    QString clickedRecipeId;
    QString expiredRecipeId;
    bool visible = false;
};
