#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

// ActionRequest 描述“最终要让运行时做什么”。
//
// 上层事件、BehaviorRule、Custom Interaction 都只能通过这个结构请求播放。
// PetRuntime 只执行请求，不再关心请求来自单击、菜单还是 agent 状态。

// InterruptHint 控制本请求遇到正在播放的动作时如何处理。
// Immediate：立即打断当前动作；AfterCurrent：等当前动作播完再切换。
enum class InterruptHint
{
    Immediate,
    AfterCurrent,
};

// ActionRequestKind 是运行时识别的全部请求类型。
// 新增类型必须同步在 PetRuntime::submitActionRequest 加分支。
enum class ActionRequestKind
{
    None,           // 空请求，pipeline 会忽略它
    ActionPool,     // 从指定 pool 按权重抽一项执行（targetId = poolId）
    Recipe,         // 播放一个 recipe 时间线（targetId = recipeId）
    Action,         // 播放一个 action（targetId = actionId）
    ReturnToIdle,   // 返回 idle 状态；当前已在 idle 时是 no-op，避免重启动画
    ToggleFacing,   // 切换朝向（right ↔ left）
    SpawnProp,      // 生成 manifest 声明的 Prop（targetId = propId，可在 options 覆盖 delayMs/durationMs）
    PlaySound,      // 直接播放一段音效（targetId = url 字符串）
};

// ActionRequest 是请求的不可变载荷。建议使用静态工厂函数构造，
// 直接构造时务必同步设置 kind 和 targetId / options。
struct ActionRequest
{
    ActionRequestKind kind = ActionRequestKind::None;
    InterruptHint interruptHint = InterruptHint::Immediate;
    QString petState;
    QString targetId;
    QVariantMap options;
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

    static ActionRequest spawnProp(const QString &propId, const QVariantMap &overrides = {})
    {
        ActionRequest request;
        request.kind = ActionRequestKind::SpawnProp;
        request.targetId = propId.trimmed();
        request.options = overrides;
        return request;
    }

    static ActionRequest playSound(const QUrl &url)
    {
        ActionRequest request;
        request.kind = ActionRequestKind::PlaySound;
        request.targetId = url.toString();
        return request;
    }

    // 标记：执行此请求时，先把当前显示的 Prop 隐藏。
    // 用于点击徽章后接 bow 动作时同步清除徽章。
    ActionRequest withHiddenCurrentProp() const
    {
        ActionRequest copy = *this;
        copy.hideCurrentProp = true;
        return copy;
    }

    ActionRequest withInterruptHint(InterruptHint hint) const
    {
        ActionRequest copy = *this;
        copy.interruptHint = hint;
        return copy;
    }

    ActionRequest withPetState(const QString &state) const
    {
        ActionRequest copy = *this;
        copy.petState = state.trimmed();
        return copy;
    }
};
