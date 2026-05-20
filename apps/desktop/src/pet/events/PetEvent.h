#pragma once

#include <QString>

// PetEvent 描述“发生了什么”，不描述要播放什么。
//
// 鼠标、菜单、动画循环、Prop 生命周期和后续 agent 状态都会先进入这一层。
// InteractionPipeline 再根据当前 RuntimeSnapshot 和皮肤配置，把事件转换为
// ActionRequest。这样 QML 不需要知道 action pool、recipe 或 GIF 资源。
enum class PetEventType
{
    PointerSingleClick,
    PointerDoubleClick,
    PointerDragShake,
    PointerDragReleased,
    MenuCommand,
    IdleLoopFinished,
    RuntimeStarted,
    ActionCompleted,
    PropClicked,
    PropExpired,
    AgentExpressionRequested,
};

struct PetEvent
{
    PetEventType type = PetEventType::MenuCommand;
    QString commandId;
    QString propId;
    QString state;
    QString expression;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double randomValue = 0.0;
    bool hasRandomValue = false;
    bool dragHoldCompleted = false;

    static PetEvent pointerSingleClick(double clickX, double clickY, double windowWidth, double windowHeight)
    {
        PetEvent event;
        event.type = PetEventType::PointerSingleClick;
        event.x = clickX;
        event.y = clickY;
        event.width = windowWidth;
        event.height = windowHeight;
        return event;
    }

    static PetEvent pointerDoubleClick()
    {
        PetEvent event;
        event.type = PetEventType::PointerDoubleClick;
        return event;
    }

    static PetEvent pointerDoubleClick(double random)
    {
        PetEvent event = pointerDoubleClick();
        event.randomValue = random;
        event.hasRandomValue = true;
        return event;
    }

    static PetEvent pointerDragShake()
    {
        PetEvent event;
        event.type = PetEventType::PointerDragShake;
        return event;
    }

    static PetEvent pointerDragReleased(bool holdCompleted)
    {
        PetEvent event;
        event.type = PetEventType::PointerDragReleased;
        event.dragHoldCompleted = holdCompleted;
        return event;
    }

    static PetEvent menuCommand(const QString &id)
    {
        PetEvent event;
        event.type = PetEventType::MenuCommand;
        event.commandId = id.trimmed();
        return event;
    }

    static PetEvent idleLoopFinished(double random)
    {
        PetEvent event;
        event.type = PetEventType::IdleLoopFinished;
        event.randomValue = random;
        event.hasRandomValue = true;
        return event;
    }

    static PetEvent runtimeStarted()
    {
        PetEvent event;
        event.type = PetEventType::RuntimeStarted;
        return event;
    }

    static PetEvent actionCompleted(double random)
    {
        PetEvent event;
        event.type = PetEventType::ActionCompleted;
        event.randomValue = random;
        event.hasRandomValue = true;
        return event;
    }

    static PetEvent propClicked(const QString &id)
    {
        PetEvent event;
        event.type = PetEventType::PropClicked;
        event.propId = id.trimmed();
        return event;
    }

    static PetEvent propExpired(const QString &id)
    {
        PetEvent event;
        event.type = PetEventType::PropExpired;
        event.propId = id.trimmed();
        return event;
    }

    static PetEvent agentExpressionRequested(const QString &stateId, const QString &expressionId, double random)
    {
        PetEvent event;
        event.type = PetEventType::AgentExpressionRequested;
        event.state = stateId.trimmed();
        event.expression = expressionId.trimmed();
        event.randomValue = random;
        event.hasRandomValue = true;
        return event;
    }
};
