#pragma once

#include <QString>

// PetEvent 描述“发生了什么”，不描述要播放什么。
//
// 鼠标、菜单、动画循环、Prop 生命周期和后续 agent 状态都会先进入这一层。
// InteractionPipeline 再根据当前 RuntimeSnapshot 和皮肤配置，把事件转换为
// ActionRequest。这样 QML 不需要知道 action pool、recipe 或 GIF 资源。

// PetEventType 把所有进入交互管线的事件来源归到一个有限集合。
// 新增类型必须同时在 InteractionPipeline 加分支，否则事件会被丢弃。
enum class PetEventType
{
    PointerSingleClick,        // 单击桌宠：x/y/width/height 描述点击位置和窗口尺寸
    PointerDoubleClick,        // 双击桌宠：可带 randomValue 触发概率玩法
    PointerDragShake,          // 拖拽时连续左右晃动达到阈值（由 GestureTracker 识别）
    PointerDragReleased,       // 拖拽松手：dragHoldCompleted 区分蹲下动画是否播完
    MenuCommand,               // 右键菜单命令：commandId 区分通用 runtime.* 与皮肤定制
    IdleLoopFinished,          // idle 动画完成一轮循环，可触发"随机抽一个空闲动作"
    RuntimeStarted,            // 桌宠启动时触发一次（用于公文包入场等启动 recipe）
    ActionCompleted,           // 任何动作播完后触发，配合 behaviorTriggers 续接下一个
    PropClicked,               // 用户点击了主体外的 Prop（如徽章）
    PropExpired,               // Prop 自然到期消失
    AgentExpressionRequested,  // 模型 / agent 提交一个表达标签，由 ExpressionMapping 解析
};

// PetEvent 是事件的不可变载荷。字段按 type 选择填充，不需要的留默认值。
// 一组静态工厂函数（pointerSingleClick / pointerDoubleClick 等）是首选构造方式。
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
