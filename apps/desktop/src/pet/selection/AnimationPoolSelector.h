#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QHash>
#include <QString>

// AnimationPoolSelector 负责从 animationPools 里选择下一条动作请求。
//
// 它不播放动画，也不改运行时状态；只处理两件稳定规则：
// 1. 按当前语音语言选择覆盖池，例如 doubleClick.random.zh。
// 2. 按 manifest 中的 weight 从候选项里抽取一条 entry。
class AnimationPoolSelector
{
public:
    static QString resolvePoolId(
        const QHash<QString, AnimationPoolDefinition> &animationPools,
        const QString &poolId,
        const QString &languageId
    );
    static AnimationPoolEntry selectEntry(const AnimationPoolDefinition &pool);
};
