#pragma once

#include "pet/events/PetEvent.h"
#include "pet/interaction/GestureTracker.h"

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class PetRuntime;

// PetEventBridge 是 QML 的行为事件入口。
//
// QML 可以读取 PetRuntime 的显示状态，但鼠标、菜单、Prop、idle loop 等行为
// 都应先提交到这里。Bridge 负责组装 PetEvent，并把管线输出的 ActionRequest
// 提交给 PetRuntime。
class PetEventBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList enabledSkinCommands READ enabledSkinCommands NOTIFY skinCommandAvailabilityChanged)

public:
    explicit PetEventBridge(PetRuntime *runtime, QObject *parent = nullptr);

    QVariantList enabledSkinCommands() const;

    Q_INVOKABLE void submitPrimaryClick(double x, double y, double width, double height);
    Q_INVOKABLE void submitDoubleClick();
    Q_INVOKABLE void submitDragStarted(double globalX);
    Q_INVOKABLE void submitDragMoved(double globalX);
    Q_INVOKABLE void submitDragEnded();
    Q_INVOKABLE void submitHoldAnimationReachedEnd();
    Q_INVOKABLE void submitMenuCommand(const QString &commandId);
    Q_INVOKABLE void submitIdleLoopFinished();
    Q_INVOKABLE void submitPropClicked();
    Q_INVOKABLE void submitPropExpired();
    void submitDoubleClickForTest(double randomValue);
    void submitIdleLoopFinishedForTest(double randomValue);

signals:
    void skinCommandAvailabilityChanged();

private:
    void submitEvent(const PetEvent &event);

    PetRuntime *m_runtime = nullptr;
    GestureTracker m_gestureTracker;
};

// 这个 wrapper 让 QML 看到 PetEventBridge 单例，但对象生命周期仍由 main.cpp 管理。
struct PetEventBridgeForeign
{
    Q_GADGET
    QML_FOREIGN(PetEventBridge)
    QML_NAMED_ELEMENT(PetEventBridge)
    QML_SINGLETON

public:
    inline static PetEventBridge *s_instance = nullptr;

    static PetEventBridge *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());

        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
