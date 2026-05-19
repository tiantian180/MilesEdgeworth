#pragma once

#include <QString>

// RuntimeSnapshot 是交互层观察运行时状态的只读视图。
//
// InteractionPipeline 只能读这个快照，不能直接改 PetRuntime。这样单击、
// 菜单和 Prop 事件可以被纯逻辑测试，也避免把事件判断继续塞回 Runtime。
struct RuntimeSnapshot
{
    QString currentState;
    QString currentActionId;
    QString currentRecipeId;
    QString currentPhaseId;
    QString currentFacing;
    QString currentPropId;
    QString currentPropClickedRecipeId;
    QString currentPropExpiredRecipeId;
    bool currentPropVisible = false;
    bool pointerInteractionEnabled = true;
    bool sleeping = false;
    bool sleepTransitioning = false;
};
