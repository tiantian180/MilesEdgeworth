#pragma once

#include "chat/ChatStreamEvent.h"
#include "pet/requests/ActionRequest.h"

#include <QJSEngine>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class ChatTextPacer;
class QNetworkReply;
class PetRuntime;
class SettingsService;

class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sidecarReady READ sidecarReady NOTIFY sidecarReadyChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)

public:
    enum class ChatPhase {
        IDLE,
        BUFFERING_FOR_START,
        STREAMING,
        GATED,
        WAITING_FOR_ANIMATION_END,
    };

    explicit ChatController(PetRuntime *runtime, SettingsService *settings, QObject *parent = nullptr);
    ~ChatController() override;

    bool sidecarReady() const { return m_sidecarReady; }
    bool sending() const { return m_sending; }
    QString statusText() const { return m_statusText; }
    QVariantList messages() const { return m_messages; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void startSidecar();
    Q_INVOKABLE void restartSidecar();
    Q_INVOKABLE void checkHealth();
    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void cancelCurrentReply();

    void applyStreamEvent(const ChatStreamEvent &event);

public slots:
    void handleSettingsSaved();

signals:
    void sidecarReadyChanged();
    void sendingChanged();
    void statusTextChanged();
    void messagesChanged();
    void openWindowRequested();

private:
    QVariantMap messageObject(const QString &role, const QString &text, bool pending, bool error) const;
    void appendMessage(const QVariantMap &message);
    void transitionTo(ChatPhase next);
    void handleCleanFinishReady();
    void handleBoundaryReached();
    void handleGateTimeout();
    void drainHoldBufferToPacer();
    void appendChunkToCurrentMessage(const QString &chunk);
    void setSidecarReady(bool ready);
    void setSending(bool sending);
    void setStatusText(const QString &statusText);
    void requestPetExpression(
        const QString &state,
        const QString &expression,
        InterruptHint interruptHint = InterruptHint::Immediate
    );
    QString sidecarExecutablePath() const;
    void launchSidecarProcess();
    void scheduleSidecarStart(int delayMs);
    void handleStreamBytes(const QByteArray &bytes);
    void finishCurrentReply();
    void failCurrentReply(const QString &message);

    PetRuntime *m_runtime = nullptr;
    SettingsService *m_settings = nullptr;
    QNetworkAccessManager m_network;
    QProcess m_sidecarProcess;
    QPointer<QNetworkReply> m_currentReply;
    ChatStreamEventParser m_parser;
    QVariantList m_messages;
    // Phase 2.3.1: while m_phase is BUFFERING_FOR_START or GATED, streamed
    // text accumulates here. On transition to STREAMING, it drains into the
    // pacer. See docs/v2/设计方案/AI 聊天动画编排设计.md §5.
    QString m_holdBuffer;
    ChatPhase m_phase = ChatPhase::IDLE;
    QString m_pendingState;
    QString m_pendingExpression;
    ChatTextPacer *m_pacer = nullptr;
    QTimer m_gateTimeout;
    static constexpr int kGateTimeoutMs = 800;
    bool m_sidecarReady = false;
    bool m_sending = false;
    bool m_sidecarRestartPending = false;
    bool m_sidecarStoppingForRestart = false;
    int m_sidecarRestartAttempts = 0;
    // 用户取消后，剩余 SSE chunks 必须被丢弃，否则会拼到新建的 assistant 消息里产生"幽灵回复"。
    // 每次 sendMessage 复位 false。
    bool m_cancelled = false;
    QString m_statusText = QStringLiteral("未连接");
    int m_assistantMessageIndex = -1;
};

struct ChatControllerForeign
{
    Q_GADGET
    QML_FOREIGN(ChatController)
    QML_NAMED_ELEMENT(ChatController)
    QML_SINGLETON

public:
    // 与 PetRuntimeForeign / PetEventBridgeForeign / DesktopShellControllerForeign 保持一致，使用 inline static 就地定义。
    inline static ChatController *s_instance = nullptr;

    static ChatController *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());

        // 单例对象由 main.cpp 持有，QML 引擎只借用，不负责 delete。
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
