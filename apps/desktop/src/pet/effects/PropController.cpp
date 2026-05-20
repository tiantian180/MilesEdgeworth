#include "pet/effects/PropController.h"

#include <QTimer>
#include <QtGlobal>

PropController::PropController(QObject *parent)
    : QObject(parent)
{
}

const PropState &PropController::current() const
{
    return m_current;
}

PropState PropController::snapshot() const
{
    return m_current;
}

int PropController::playbackSerial() const
{
    return m_playbackSerial;
}

void PropController::scheduleForRecipe(
    const SkinManifest &manifest,
    const RecipeDefinition &recipe,
    const QString &facing,
    double petScale
)
{
    if (recipe.propId.isEmpty() || !manifest.props.contains(recipe.propId)) {
        // 新 recipe 没有 Prop 时，取消仍在延迟队列里的旧 Prop。
        ++m_requestSerial;
        return;
    }

    const PropDefinition prop = manifest.props.value(recipe.propId);
    const QString defaultFacing = manifest.defaultFacing;
    const int requestSerial = ++m_requestSerial;

    // Prop 可以延迟出现，例如先播放出手动作，再飞出附件。
    // requestSerial 用来避免用户快速切动作后冒出上一轮旧 Prop。
    QTimer::singleShot(qMax(0, prop.delayMs), this, [this, prop, facing, defaultFacing, petScale, requestSerial]() {
        if (requestSerial != m_requestSerial) {
            return;
        }

        spawn(prop, facing, defaultFacing, petScale);
    });
}

void PropController::spawnFromRequest(
    const SkinManifest &manifest,
    const QString &propId,
    const QString &facing,
    double petScale,
    const QVariantMap &overrides
)
{
    if (propId.isEmpty() || !manifest.props.contains(propId)) {
        ++m_requestSerial;
        return;
    }

    PropDefinition prop = manifest.props.value(propId);
    if (overrides.contains(QStringLiteral("delayMs"))) {
        bool ok = false;
        const int delayMs = overrides.value(QStringLiteral("delayMs")).toInt(&ok);
        if (ok) {
            prop.delayMs = delayMs;
        }
    }
    if (overrides.contains(QStringLiteral("durationMs"))) {
        bool ok = false;
        const int durationMs = overrides.value(QStringLiteral("durationMs")).toInt(&ok);
        if (ok) {
            prop.durationMs = durationMs;
        }
    }

    const QString defaultFacing = manifest.defaultFacing;
    const int requestSerial = ++m_requestSerial;

    // Custom Interaction 只能请求一个 manifest 已声明的 Prop。
    // overrides 只覆盖运行时参数，不允许绕过皮肤包的资源和尺寸定义。
    QTimer::singleShot(qMax(0, prop.delayMs), this, [this, prop, facing, defaultFacing, petScale, requestSerial]() {
        if (requestSerial != m_requestSerial) {
            return;
        }

        spawn(prop, facing, defaultFacing, petScale);
    });
}

void PropController::hide()
{
    ++m_requestSerial;

    if (!m_current.visible && m_current.id.isEmpty()) {
        return;
    }

    m_current = {};
    emit currentPropChanged();
}

void PropController::spawn(const PropDefinition &prop, const QString &facing, const QString &defaultFacing, double petScale)
{
    const QPointF startOffset = scaledPoint(pointForFacing(prop.startOffsets, facing, defaultFacing), petScale);
    const QPointF delta = travelDelta(prop, facing, defaultFacing, petScale);

    m_current.visible = true;
    m_current.id = prop.id;
    m_current.imageUrl = prop.assetUrl;
    m_current.startOffset = startOffset;
    m_current.endOffset = startOffset + delta;
    m_current.width = scaledLength(prop.width > 0 ? prop.width : 94, petScale);
    m_current.height = scaledLength(prop.height > 0 ? prop.height : 94, petScale);
    m_current.visualWidth = scaledLength(prop.visualWidth > 0 ? prop.visualWidth : (prop.width > 0 ? prop.width : 94), petScale);
    m_current.visualHeight = scaledLength(prop.visualHeight > 0 ? prop.visualHeight : (prop.height > 0 ? prop.height : 94), petScale);
    m_current.durationMs = prop.durationMs > 0 ? prop.durationMs : 1500;
    m_current.clickedRecipeId = prop.clickedRecipeId;
    m_current.expiredRecipeId = prop.expiredRecipeId;
    ++m_playbackSerial;

    emit currentPropChanged();
    emit currentPropPlaybackSerialChanged();
}

double PropController::scaledLength(double length, double petScale) const
{
    // Prop 配置按默认中号 scale=2 记录，实际显示按当前宠物比例缩放。
    const double safeScale = petScale > 0.0 ? petScale : 2.0;
    return length * safeScale / 2.0;
}

QPointF PropController::pointForFacing(
    const QHash<QString, QPointF> &points,
    const QString &facing,
    const QString &defaultFacing
) const
{
    if (points.contains(facing)) {
        return points.value(facing);
    }
    if (points.contains(defaultFacing)) {
        return points.value(defaultFacing);
    }
    if (!points.isEmpty()) {
        return points.constBegin().value();
    }
    return {};
}

QPointF PropController::scaledPoint(const QPointF &point, double petScale) const
{
    return QPointF(scaledLength(point.x(), petScale), scaledLength(point.y(), petScale));
}

QPointF PropController::travelDelta(
    const PropDefinition &prop,
    const QString &facing,
    const QString &defaultFacing,
    double petScale
) const
{
    if (!prop.travelBaseDeltas.isEmpty() || !prop.travelPerScaleDeltas.isEmpty()) {
        const QPointF base = pointForFacing(prop.travelBaseDeltas, facing, defaultFacing);
        const QPointF perScale = pointForFacing(prop.travelPerScaleDeltas, facing, defaultFacing);
        const double safeScale = petScale > 0.0 ? petScale : 2.0;
        return base + perScale * safeScale;
    }

    return scaledPoint(pointForFacing(prop.travelDeltas, facing, defaultFacing), petScale);
}
