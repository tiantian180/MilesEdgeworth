#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

// HitZoneMatchContext 描述一次点击命中计算所需的运行时上下文。
//
// hitZones 目前以 Miles 皮肤的 240x240 逻辑画布为基准声明。QML 传入的是
// 真实窗口坐标，matcher 会先归一化到这个逻辑画布，再按当前朝向选择区域。
struct HitZoneMatchContext
{
    QString facing;
    QString defaultFacing;
    double canvasWidth = 240.0;
    double canvasHeight = 240.0;
};

// HitZoneMatcher 只负责“点中了哪个单击动作池”的纯匹配逻辑。
//
// 它不播放动画、不读取 manifest 文件、不修改 PetRuntime 状态。这样后续把
// 单击、双击、拖拽等交互统一迁入 InteractionController 时，可以复用这里的
// 坐标和区域判断。
class HitZoneMatcher
{
public:
    static QString clickPoolForPoint(
        const SkinManifest &manifest,
        const HitZoneMatchContext &context,
        double x,
        double y,
        double width,
        double height
    );

    static QRectF rectForHitZone(
        const HitZoneDefinition &zone,
        const HitZoneMatchContext &context
    );

    static QList<QPointF> polygonForHitZone(
        const HitZoneDefinition &zone,
        const HitZoneMatchContext &context
    );

    static bool hitZoneContainsPoint(
        const HitZoneDefinition &zone,
        const HitZoneMatchContext &context,
        const QPointF &point
    );
};
