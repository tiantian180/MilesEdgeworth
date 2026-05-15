#pragma once

#include <QHash>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// PetRuntime 是 v2 桌宠动画系统的最小入口。
//
// 当前阶段它只做三件事：
// 1. 从内置 skin manifest 里读出 state -> action -> animation 的映射。
// 2. 暴露 currentAnimationUrl 给 QML 的 AnimatedImage 使用。
// 3. 提供几个开发测试入口，方便右键菜单验证状态切换。
//
// 更复杂的动作编排、enter/loop/exit、移动驱动动画、点击交互优先级，
// 后续会在 Pet Runtime / Animation Orchestrator 里继续分层扩展。
class PetRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentState READ currentState NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentActionId READ currentActionId NOTIFY currentActionChanged)
    Q_PROPERTY(QUrl currentAnimationUrl READ currentAnimationUrl NOTIFY currentAnimationUrlChanged)

public:
    explicit PetRuntime(QObject *parent = nullptr);

    QString currentState() const;
    QString currentActionId() const;
    QUrl currentAnimationUrl() const;

    Q_INVOKABLE void setState(const QString &state);
    Q_INVOKABLE void playAction(const QString &actionId);
    Q_INVOKABLE void returnToIdle();
    Q_INVOKABLE void testThinking();
    Q_INVOKABLE void testSpeaking();

signals:
    void currentStateChanged();
    void currentActionChanged();
    void currentAnimationUrlChanged();

private:
    struct ActionDefinition
    {
        QUrl animationUrl;
        bool loop = true;
    };

    void loadManifest();
    void loadFallbackManifest();
    QString actionForState(const QString &state) const;
    void setCurrentAction(const QString &actionId, const ActionDefinition &action);

    QHash<QString, QString> m_stateToAction;
    QHash<QString, ActionDefinition> m_actions;
    QString m_fallbackAction = "idle_stand";
    QString m_currentState = "idle";
    QString m_currentActionId;
    QUrl m_currentAnimationUrl;
};

// 与 DesktopShellControllerForeign 一样，这个 wrapper 让 QML 看到一个名为
// PetRuntime 的单例，但真实对象仍由 main.cpp 创建和持有。
struct PetRuntimeForeign
{
    Q_GADGET
    QML_FOREIGN(PetRuntime)
    QML_NAMED_ELEMENT(PetRuntime)
    QML_SINGLETON

public:
    inline static PetRuntime *s_instance = nullptr;

    static PetRuntime *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());

        // QML 只借用桌宠运行时；C++ 负责生命周期，避免引擎退出时误删栈对象。
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
