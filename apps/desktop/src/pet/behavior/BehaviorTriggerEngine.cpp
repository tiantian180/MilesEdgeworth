#include "pet/behavior/BehaviorTriggerEngine.h"

#include <QtGlobal>

bool BehaviorTriggerEngine::matches(const BehaviorTriggerDefinition &trigger, const BehaviorTriggerContext &context)
{
    if (!trigger.state.isEmpty() && trigger.state != context.state) {
        return false;
    }

    if (!trigger.actionId.isEmpty() && trigger.actionId != context.actionId) {
        return false;
    }

    if (trigger.requiresNoActiveRecipe && context.hasActiveRecipe) {
        return false;
    }

    return true;
}

BehaviorTriggerEntry BehaviorTriggerEngine::selectEntry(const BehaviorTriggerDefinition &trigger, double randomValue)
{
    BehaviorTriggerEntry fallbackEntry;
    if (trigger.entries.isEmpty()) {
        return fallbackEntry;
    }

    int totalWeight = 0;
    for (const BehaviorTriggerEntry &entry : trigger.entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight <= 0) {
        return trigger.entries.constFirst();
    }

    // 外部测试会传入确定性的 0..1 随机值；真实运行时则来自 QRandomGenerator。
    // 这里把它投射到权重区间，manifest 里的 70/30、80/20 等比例都能复用同一套逻辑。
    double cursor = qBound(0.0, randomValue, 0.999999999) * totalWeight;
    for (const BehaviorTriggerEntry &entry : trigger.entries) {
        cursor -= entry.weight;
        if (cursor < 0) {
            return entry;
        }
    }

    return trigger.entries.constLast();
}
