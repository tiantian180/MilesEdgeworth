#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QString>

// BehaviorTriggerContext 是触发规则需要观察的运行时上下文。
// 它把 PetRuntime 的状态压缩成纯数据，方便触发引擎独立测试和后续复用。
struct BehaviorTriggerContext
{
    QString state;
    QString actionId;
    bool hasActiveRecipe = false;
};

// BehaviorTriggerEngine 负责判断一个行为触发器是否可执行，并从结果里抽取一项。
//
// 它不直接调用 playAction / playRecipe；PetRuntime 收到返回 entry 后再决定怎么执行。
class BehaviorTriggerEngine
{
public:
    static bool matches(const BehaviorTriggerDefinition &trigger, const BehaviorTriggerContext &context);
    static BehaviorTriggerEntry selectEntry(
        const BehaviorTriggerDefinition &trigger,
        const BehaviorTriggerContext &context,
        double randomValue
    );
};
