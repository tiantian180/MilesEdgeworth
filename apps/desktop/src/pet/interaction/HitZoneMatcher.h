#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

// HitZoneMatchContext 描述一次点击命中计算所需的运行时上下文。
//
// hitZones 以皮肤 manifest 声明的逻辑画布为基准。QML 传入真实窗口坐标，
// matcher 会先归一化到这个逻辑画布，再按当前朝向选择区域。
struct HitZoneMatchContext
{
    QString facing;
    QString defaultFacing;
    double canvasWidth = 0.0;
    double canvasHeight = 0.0;
};

// HitZoneMatcher 只负责“点中了哪个 hit zone”的纯匹配逻辑。
//
// 它不播放动画、不读取 manifest 文件、不修改 PetRuntime 状态。这样后续把
// 单击、双击、拖拽等交互统一迁入 InteractionController 时，可以复用这里的
// 坐标和区域判断。
class HitZoneMatcher
{
public:
    static QString hitZoneIdForPoint(
        const SkinManifest &manifest,
        const HitZoneMatchContext &context,
        const QStringList &candidateZoneIds,
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
