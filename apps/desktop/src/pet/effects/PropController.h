#pragma once

#include "pet/effects/PropState.h"
#include "pet/manifest/SkinManifest.h"

#include <QObject>
#include <QVariantMap>

class PropController : public QObject
{
    Q_OBJECT

public:
    explicit PropController(QObject *parent = nullptr);

    const PropState &current() const;
    PropState snapshot() const;
    int playbackSerial() const;

    void scheduleForRecipe(
        const SkinManifest &manifest,
        const RecipeDefinition &recipe,
        const QString &facing,
        double petScale
    );
    void spawnFromRequest(
        const SkinManifest &manifest,
        const QString &propId,
        const QString &facing,
        double petScale,
        const QVariantMap &overrides = {}
    );
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
