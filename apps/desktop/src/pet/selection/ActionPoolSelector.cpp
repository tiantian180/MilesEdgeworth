#include "pet/selection/ActionPoolSelector.h"

#include <QRandomGenerator>

QString ActionPoolSelector::resolvePoolId(
    const QHash<QString, ActionPoolDefinition> &actionPools,
    const QString &poolId,
    const QString &voiceLanguage
)
{
    const QString normalizedPoolId = poolId.trimmed();
    if (normalizedPoolId.isEmpty()) {
        return {};
    }

    // 语言后缀池用于还原旧版“某些语言没有某个语音动作”的细节。
    // 例如中文没有 eureka2.wav，manifest 可以用 doubleClick.random.zh
    // 覆盖默认 doubleClick.random。
    const QString languagePoolId = normalizedPoolId + "." + voiceLanguage;
    if (actionPools.contains(languagePoolId)) {
        return languagePoolId;
    }

    return normalizedPoolId;
}

ActionPoolEntry ActionPoolSelector::selectEntry(const ActionPoolDefinition &pool)
{
    ActionPoolEntry fallbackEntry;
    if (pool.entries.isEmpty()) {
        return fallbackEntry;
    }

    int totalWeight = 0;
    for (const ActionPoolEntry &entry : pool.entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight <= 0) {
        return pool.entries.constFirst();
    }

    int cursor = QRandomGenerator::global()->bounded(totalWeight);
    for (const ActionPoolEntry &entry : pool.entries) {
        cursor -= entry.weight;
        if (cursor < 0) {
            return entry;
        }
    }

    return pool.entries.constLast();
}
