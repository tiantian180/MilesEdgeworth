#include "pet/selection/AnimationPoolSelector.h"

#include <QRandomGenerator>

QString AnimationPoolSelector::resolvePoolId(
    const QHash<QString, AnimationPoolDefinition> &animationPools,
    const QString &poolId,
    const QString &languageId
)
{
    const QString normalizedPoolId = poolId.trimmed();
    if (normalizedPoolId.isEmpty()) {
        return {};
    }

    // 语言后缀池用于还原旧版“某些语言没有某个语音动作”的细节。
    // 例如中文没有 eureka2.wav，manifest 可以用 doubleClick.random.zh
    // 覆盖默认 doubleClick.random。
    const QString languagePoolId = normalizedPoolId + "." + languageId;
    if (animationPools.contains(languagePoolId)) {
        return languagePoolId;
    }

    return normalizedPoolId;
}

AnimationPoolEntry AnimationPoolSelector::selectEntry(const AnimationPoolDefinition &pool)
{
    AnimationPoolEntry fallbackEntry;
    if (pool.entries.isEmpty()) {
        return fallbackEntry;
    }

    int totalWeight = 0;
    for (const AnimationPoolEntry &entry : pool.entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight <= 0) {
        return pool.entries.constFirst();
    }

    int cursor = QRandomGenerator::global()->bounded(totalWeight);
    for (const AnimationPoolEntry &entry : pool.entries) {
        cursor -= entry.weight;
        if (cursor < 0) {
            return entry;
        }
    }

    return pool.entries.constLast();
}
