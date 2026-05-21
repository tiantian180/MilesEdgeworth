#include "pet/interaction/HitZoneMatcher.h"

namespace {
bool pointInPolygon(const QList<QPointF> &polygon, const QPointF &point)
{
    if (polygon.size() < 3) {
        return false;
    }

    // 使用射线法判断点是否在多边形内。旧版脸部区域有斜线边界，
    // polygon 能比粗矩形更准确地还原“点到脸才害怕”的手感。
    bool inside = false;
    int previousIndex = polygon.size() - 1;
    for (int currentIndex = 0; currentIndex < polygon.size(); ++currentIndex) {
        const QPointF current = polygon.at(currentIndex);
        const QPointF previous = polygon.at(previousIndex);
        const bool yCrosses = ((current.y() > point.y()) != (previous.y() > point.y()));
        if (yCrosses) {
            const double xAtPointY = (previous.x() - current.x()) * (point.y() - current.y())
                / (previous.y() - current.y()) + current.x();
            if (point.x() < xAtPointY) {
                inside = !inside;
            }
        }

        previousIndex = currentIndex;
    }

    return inside;
}
} // namespace

QString HitZoneMatcher::hitZoneIdForPoint(
    const SkinManifest &manifest,
    const HitZoneMatchContext &context,
    const QStringList &candidateZoneIds,
    double x,
    double y,
    double width,
    double height
)
{
    if (width <= 0 || height <= 0 || context.canvasWidth <= 0 || context.canvasHeight <= 0) {
        return {};
    }

    // QML 传入真实窗口尺寸；manifest 用逻辑画布描述区域。
    // 这里集中归一化，后续皮肤调整 canvas 时不需要改 QML 点击入口。
    const QPointF logicalPoint(
        x * context.canvasWidth / width,
        y * context.canvasHeight / height
    );

    const QStringList orderedZoneIds = candidateZoneIds.isEmpty()
        ? manifest.hitZones.keys()
        : candidateZoneIds;
    for (const QString &zoneId : orderedZoneIds) {
        const HitZoneDefinition zone = manifest.hitZones.value(zoneId);
        if (!zone.id.isEmpty() && hitZoneContainsPoint(zone, context, logicalPoint)) {
            return zoneId;
        }
    }

    return {};
}

QRectF HitZoneMatcher::rectForHitZone(
    const HitZoneDefinition &zone,
    const HitZoneMatchContext &context
)
{
    if (zone.facingRects.contains(context.facing)) {
        return zone.facingRects.value(context.facing);
    }

    if (zone.rect.isValid()) {
        return zone.rect;
    }

    return zone.facingRects.value(context.defaultFacing, QRectF());
}

QList<QPointF> HitZoneMatcher::polygonForHitZone(
    const HitZoneDefinition &zone,
    const HitZoneMatchContext &context
)
{
    if (zone.facingPolygons.contains(context.facing)) {
        return zone.facingPolygons.value(context.facing);
    }

    if (zone.polygon.size() >= 3) {
        return zone.polygon;
    }

    return zone.facingPolygons.value(context.defaultFacing);
}

bool HitZoneMatcher::hitZoneContainsPoint(
    const HitZoneDefinition &zone,
    const HitZoneMatchContext &context,
    const QPointF &point
)
{
    const QList<QPointF> polygon = polygonForHitZone(zone, context);
    if (polygon.size() >= 3) {
        return pointInPolygon(polygon, point);
    }

    return rectForHitZone(zone, context).contains(point);
}
