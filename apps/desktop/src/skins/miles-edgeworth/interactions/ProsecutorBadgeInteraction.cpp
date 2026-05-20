#include "skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h"

#include <QUrl>
#include <QVariantMap>

namespace {
constexpr auto kInteractionId = "prosecutor_badge";

QString configString(const QVariantMap &config, const QString &key)
{
    return config.value(key).toString().trimmed();
}

int configInt(const QVariantMap &config, const QString &key, int fallback)
{
    bool ok = false;
    const int value = config.value(key).toInt(&ok);
    return ok ? value : fallback;
}

double configDouble(const QVariantMap &config, const QString &key, double fallback)
{
    bool ok = false;
    const double value = config.value(key).toDouble(&ok);
    return ok ? value : fallback;
}
} // namespace

QString ProsecutorBadgeInteraction::id() const
{
    return QString::fromUtf8(kInteractionId);
}

QSet<PetEventType> ProsecutorBadgeInteraction::supportedEvents() const
{
    return {
        PetEventType::PointerDoubleClick,
        PetEventType::PropClicked,
        PetEventType::PropExpired,
    };
}

CustomInteractionOutcome ProsecutorBadgeInteraction::handleEvent(
    const PetEvent &event,
    const RuntimeSnapshot &snapshot,
    CustomInteractionHostApi &host
)
{
    const QVariantMap config = host.manifestConfig();
    const QString badgePropId = configString(config, QStringLiteral("badgePropId"));
    if (badgePropId.isEmpty()) {
        return {};
    }

    switch (event.type) {
    case PetEventType::PointerDoubleClick: {
        // 睡眠、过渡和禁止鼠标交互等通用状态交还默认逻辑处理。
        if (!snapshot.pointerInteractionEnabled || snapshot.sleeping || snapshot.sleepTransitioning) {
            return {};
        }

        const double probability = configDouble(config, QStringLiteral("takeThatProbability"), 0.0);
        if (probability <= 0.0 || host.random() >= probability) {
            return {};
        }

        const QString objectingAction = configString(config, QStringLiteral("objectingAction"));
        const QString soundUrl = configString(config, QStringLiteral("takeThatSound"));
        const int badgeDelayMs = configInt(config, QStringLiteral("badgeDelayMs"), 0);

        if (!objectingAction.isEmpty()) {
            host.emitAction(objectingAction);
        }
        if (!soundUrl.isEmpty()) {
            host.playSound(QUrl(soundUrl));
        }

        QVariantMap overrides;
        overrides.insert(QStringLiteral("delayMs"), badgeDelayMs);
        host.spawnProp(badgePropId, overrides);
        host.skipDefault();
        host.stopPropagation();
        return {};
    }

    case PetEventType::PropClicked:
        if (event.propId == badgePropId) {
            host.hideCurrentProp();
            host.emitRecipe(configString(config, QStringLiteral("clickedRecipe")));
            host.skipDefault();
            host.stopPropagation();
        }
        return {};

    case PetEventType::PropExpired:
        if (event.propId == badgePropId) {
            host.hideCurrentProp();
            host.emitRecipe(configString(config, QStringLiteral("expiredRecipe")));
            host.skipDefault();
            host.stopPropagation();
        }
        return {};

    default:
        return {};
    }
}
