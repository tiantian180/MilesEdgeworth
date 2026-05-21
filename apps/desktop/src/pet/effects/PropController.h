#pragma once

#include "pet/effects/PropState.h"
#include "pet/manifest/SkinManifest.h"

#include <QObject>
#include <QVariantMap>

// PropController 管理桌宠主体外的临时视觉对象（飞出的徽章、掉落物等）的生命周期。
//
// 输入只有两种：一是 RecipeDefinition 里声明的 prop（recipe 触发时按 delayMs 延后弹出），
// 二是 CustomInteraction 通过 ActionRequest::spawnProp 主动生成。
// 输出是 PropState 状态变化信号，由 PetRuntime 转发给 PetSurfaceWindow（PropSurfaceWindow）显示。
//
// requestSerial 用于"延后弹出期间用户已经取消"的情况，定时回调先比对序号再决定是否真的 spawn。
class PropController : public QObject
{
    Q_OBJECT

public:
    explicit PropController(QObject *parent = nullptr);

    // 当前 Prop 状态（visible/不可见、位置、尺寸等），供 QML 显示读取。
    const PropState &current() const;
    PropState snapshot() const;
    // 每次成功 spawn 后递增；用于 QML 触发飞行动画的"重新开始"信号。
    int playbackSerial() const;

    // recipe 包含 propId 时调用：按 manifest.props 里的 delayMs 延迟后 spawn。
    void scheduleForRecipe(
        const SkinManifest &manifest,
        const RecipeDefinition &recipe,
        const QString &facing,
        double petScale
    );

    // CustomInteraction 主动生成 Prop。
    // overrides 只能覆盖 delayMs / durationMs 这种运行时参数；图像 URL、尺寸、起点仍以 manifest 为准，避免越权。
    void spawnFromRequest(
        const SkinManifest &manifest,
        const QString &propId,
        const QString &facing,
        double petScale,
        const QVariantMap &overrides = {}
    );

    // 立即隐藏当前 Prop。点击徽章或外部强制清场时使用。
    void hide();

signals:
    void currentPropChanged();
    void currentPropPlaybackSerialChanged();

private:
    void spawn(const PropDefinition &prop, const QString &facing, const QString &defaultFacing, double petScale);
    double scaledLength(double length, double petScale) const;
    QPointF pointForFacing(const QHash<QString, QPointF> &points, const QString &facing, const QString &defaultFacing) const;
    QPointF scaledPoint(const QPointF &point, double petScale) const;
    QPointF travelDelta(const PropDefinition &prop, const QString &facing, const QString &defaultFacing, double petScale) const;

    PropState m_current;
    int m_requestSerial = 0;
    int m_playbackSerial = 0;
};
