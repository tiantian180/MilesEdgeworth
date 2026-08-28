#pragma once

#include <QString>

// RuntimeSnapshot 是交互层观察运行时状态的只读视图。
//
// InteractionPipeline 只能读这个快照，不能直接改 PetRuntime。这样单击、
// 菜单和 Prop 事件可以被纯逻辑测试，也避免把事件判断继续塞回 Runtime。
//
// 字段含义：
// - currentState：高层状态（idle / sleeping / agent.thinking 等）
// - currentActionId / currentRecipeId / currentPhaseId：正在播放的动作链路
// - currentFacing：当前朝向（与 manifest.facings 中的某一项对应）
// - currentProp*：当前 Prop 的可见性 / 点击和过期 recipe，用于把 PropClicked / PropExpired 事件转回 recipe 请求
// - pointerInteractionEnabled：当前是否接受鼠标输入（启动入场动画期间为 false）
// - sleeping / sleepTransitioning：睡眠状态机
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
    bool reducedMotion = false;
    bool sleeping = false;
    bool sleepTransitioning = false;
};
