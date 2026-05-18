#pragma once

#include <QHash>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
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
// Phase 0.6 开始引入“朝向”和“动作播放模式”，但仍然不做完整编排器。
// 后续的 enter/loop/exit、移动驱动动画、点击交互优先级，会继续在
// Pet Runtime / Animation Orchestrator 里分层扩展。
class PetRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentState READ currentState NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentActionId READ currentActionId NOTIFY currentActionChanged)
    Q_PROPERTY(QString currentPhaseId READ currentPhaseId NOTIFY currentPhaseChanged)
    Q_PROPERTY(QString currentFacing READ currentFacing NOTIFY currentFacingChanged)
    Q_PROPERTY(QString currentLoopMode READ currentLoopMode NOTIFY currentLoopModeChanged)
    Q_PROPERTY(bool currentAutoReturnToIdle READ currentAutoReturnToIdle NOTIFY currentAutoReturnToIdleChanged)
    Q_PROPERTY(QUrl currentAnimationUrl READ currentAnimationUrl NOTIFY currentAnimationUrlChanged)
    Q_PROPERTY(int playbackSerial READ playbackSerial NOTIFY playbackSerialChanged)

public:
    explicit PetRuntime(QObject *parent = nullptr);

    QString currentState() const;
    QString currentActionId() const;
    QString currentPhaseId() const;
    QString currentFacing() const;
    QString currentLoopMode() const;
    bool currentAutoReturnToIdle() const;
    QUrl currentAnimationUrl() const;
    int playbackSerial() const;

    Q_INVOKABLE void setState(const QString &state);
    Q_INVOKABLE void setFacing(const QString &facing);
    Q_INVOKABLE void toggleFacing();
    Q_INVOKABLE void playAction(const QString &actionId);
    Q_INVOKABLE void returnToIdle();
    Q_INVOKABLE void testThinking();
    Q_INVOKABLE void testSpeaking();
    Q_INVOKABLE void testObjecting();
    Q_INVOKABLE void testBow();
    Q_INVOKABLE void testTea();
    Q_INVOKABLE void testSleep();
    Q_INVOKABLE void handleAnimationFinished();

signals:
    void currentStateChanged();
    void currentActionChanged();
    void currentPhaseChanged();
    void currentFacingChanged();
    void currentLoopModeChanged();
    void currentAutoReturnToIdleChanged();
    void currentAnimationUrlChanged();
    void playbackSerialChanged();

private:
    struct PhaseDefinition
    {
        QString loopMode = "loop";
        QString nextPhase;
        QHash<QString, QUrl> variants;
    };

    struct ActionDefinition
    {
        QString label;
        QString category;
        QString loopMode = "loop";
        int priority = 0;
        QString interruptPolicy = "replace";
        QStringList tags;
        QHash<QString, QUrl> variants;
        QString initialPhase;
        QString exitPhase;
        QHash<QString, PhaseDefinition> phases;
    };

    void loadManifest();
    void loadFallbackManifest();
    QString actionForState(const QString &state) const;
    QUrl variantForFacing(const QHash<QString, QUrl> &variants, const QString &facing) const;
    void playPhase(const QString &actionId, const QString &phaseId);
    void setCurrentAction(const QString &actionId, const ActionDefinition &action);
    void setCurrentPhase(const QString &actionId, const QString &phaseId, const PhaseDefinition &phase);

    QHash<QString, QString> m_stateToAction;
    QHash<QString, ActionDefinition> m_actions;
    QString m_fallbackAction = "idle_stand";
    QStringList m_facings = {"right", "left"};
    QString m_defaultFacing = "right";
    QString m_currentState = "idle";
    QString m_currentActionId;
    QString m_currentPhaseId;
    QString m_currentFacing = "right";
    QString m_currentLoopMode = "loop";
    bool m_currentAutoReturnToIdle = false;
    QUrl m_currentAnimationUrl;
    int m_playbackSerial = 0;
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
