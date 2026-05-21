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

namespace {
bool entryMatches(const BehaviorTriggerEntry &entry, const BehaviorTriggerContext &context)
{
    // action.completed 这类触发器需要在同一个 event 下按当前 action 分支。
    // 条件放在 entry 上后，manifest 可以声明 walk/run 各自的续接池，
    // Runtime 不需要再知道任何具体动作名。
    if (!entry.when.actionId.isEmpty() && entry.when.actionId != context.actionId) {
        return false;
    }

    return true;
}
} // namespace

BehaviorTriggerEntry BehaviorTriggerEngine::selectEntry(
    const BehaviorTriggerDefinition &trigger,
    const BehaviorTriggerContext &context,
    double randomValue
)
{
    BehaviorTriggerEntry fallbackEntry;
    if (trigger.entries.isEmpty()) {
        return fallbackEntry;
    }

    QList<BehaviorTriggerEntry> entries;
    for (const BehaviorTriggerEntry &entry : trigger.entries) {
        if (entryMatches(entry, context)) {
            entries.append(entry);
        }
    }
    if (entries.isEmpty()) {
        return fallbackEntry;
    }

    int totalWeight = 0;
    for (const BehaviorTriggerEntry &entry : entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight <= 0) {
        return entries.constFirst();
    }

    // 外部测试会传入确定性的 0..1 随机值；真实运行时则来自 QRandomGenerator。
    // 这里把它投射到权重区间，manifest 里的 70/30、80/20 等比例都能复用同一套逻辑。
    double cursor = qBound(0.0, randomValue, 0.999999999) * totalWeight;
    for (const BehaviorTriggerEntry &entry : entries) {
        cursor -= entry.weight;
        if (cursor < 0) {
            return entry;
        }
    }

    return entries.constLast();
}
