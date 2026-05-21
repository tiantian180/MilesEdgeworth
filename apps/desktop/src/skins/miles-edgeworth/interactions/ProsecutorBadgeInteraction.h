#pragma once

#include "pet/interaction/CustomInteractionRegistry.h"

// ProsecutorBadgeInteraction 是 Miles 皮肤的"双击概率丢检察官徽章"玩法的代码实现。
//
// 行为：双击桌宠时，按 manifest.customInteractionConfig.prosecutor_badge.takeThatProbability
// 决定走"看招"recipe（播 doubleClick.takeThat + 飞出徽章），还是回落到默认双击池。
// 后续 PropClicked / PropExpired 事件触发鞠躬 / 捡起徽章 recipe。
// 这里完全通过 CustomInteractionHostApi 操作，不直接 import PetRuntime。
class ProsecutorBadgeInteraction final : public CustomInteraction
{
public:
    QString id() const override;
    QSet<PetEventType> supportedEvents() const override;
    CustomInteractionOutcome handleEvent(
        const PetEvent &event,
        const RuntimeSnapshot &snapshot,
        CustomInteractionHostApi &host
    ) override;
};
