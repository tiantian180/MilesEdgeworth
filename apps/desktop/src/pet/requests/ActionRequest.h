#pragma once

#include <QString>

// ActionRequest 描述“最终要让运行时做什么”。
//
// 上层事件、BehaviorRule、Custom Interaction 都只能通过这个结构请求播放。
// PetRuntime 只执行请求，不再关心请求来自单击、菜单还是 agent 状态。
enum class InterruptHint
{
    Immediate,
    AfterCurrent,
};

enum class ActionRequestKind
{
    None,
    ActionPool,
    Recipe,
    Action,
    ReturnToIdle,
    ToggleFacing,
};

struct ActionRequest
{
    ActionRequestKind kind = ActionRequestKind::None;
    InterruptHint interruptHint = InterruptHint::Immediate;
    QString targetId;
    bool hideCurrentProp = false;

    static ActionRequest none()
    {
        return {};
    }

    static ActionRequest actionPool(const QString &poolId)
    {
        ActionRequest request;
        request.kind = ActionRequestKind::ActionPool;
        request.targetId = poolId.trimmed();
        return request;
    }

    static ActionRequest recipe(const QString &recipeId)
    {
        ActionRequest request;
        request.kind = ActionRequestKind::Recipe;
        request.targetId = recipeId.trimmed();
        return request;
    }

    static ActionRequest action(const QString &actionId)
    {
        ActionRequest request;
        request.kind = ActionRequestKind::Action;
        request.targetId = actionId.trimmed();
        return request;
    }

    static ActionRequest returnToIdle()
    {
        ActionRequest request;
        request.kind = ActionRequestKind::ReturnToIdle;
        return request;
    }

    static ActionRequest toggleFacing()
    {
        ActionRequest request;
        request.kind = ActionRequestKind::ToggleFacing;
        return request;
    }

    ActionRequest withHiddenCurrentProp() const
    {
        ActionRequest copy = *this;
        copy.hideCurrentProp = true;
        return copy;
    }
};
