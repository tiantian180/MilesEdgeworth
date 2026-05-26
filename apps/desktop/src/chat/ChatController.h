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
#include <QSet>
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
    Q_PROPERTY(bool providerConfigured READ providerConfigured NOTIFY providerConfiguredChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    Q_PROPERTY(bool executingTool READ executingTool NOTIFY executingToolChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantList conversations READ conversations NOTIFY conversationsChanged)
    Q_PROPERTY(QString currentConversationId READ currentConversationId NOTIFY currentConversationIdChanged)
    Q_PROPERTY(bool conversationSkinMismatch READ conversationSkinMismatch NOTIFY conversationSkinMismatchChanged)
    Q_PROPERTY(QString conversationSkinHint READ conversationSkinHint NOTIFY conversationSkinMismatchChanged)

public:
    enum class ChatPhase {
        IDLE,
        BUFFERING_FOR_START,
        STREAMING,
        GATED,
        EXECUTING_TOOL,
        WAITING_FOR_ANIMATION_END,
    };

    explicit ChatController(PetRuntime *runtime, SettingsService *settings, QObject *parent = nullptr);
    ~ChatController() override;

    bool sidecarReady() const { return m_sidecarReady; }
    bool providerConfigured() const { return m_providerConfigured; }
    bool sending() const { return m_sending; }
    bool executingTool() const { return m_phase == ChatPhase::EXECUTING_TOOL; }
    QString statusText() const { return m_statusText; }
    QVariantList messages() const { return m_messages; }
    QVariantList conversations() const { return m_conversations; }
    QString currentConversationId() const { return m_currentConversationId; }
    bool conversationSkinMismatch() const { return m_conversationSkinMismatch; }
    QString conversationSkinHint() const { return m_conversationSkinHint; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void startSidecar();
    Q_INVOKABLE void restartSidecar();
    Q_INVOKABLE void checkHealth();
    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void cancelCurrentReply();
    Q_INVOKABLE void loadConversations();
    Q_INVOKABLE void switchConversation(const QString &id);
    Q_INVOKABLE void newConversation();
    Q_INVOKABLE void deleteConversation(const QString &id);

    void applyStreamEvent(const ChatStreamEvent &event);

public slots:
    void handleSettingsSaved();

signals:
    void sidecarReadyChanged();
    void providerConfiguredChanged();
    void sendingChanged();
    void executingToolChanged();
    void statusTextChanged();
    void messagesChanged();
    void conversationsChanged();
    void currentConversationIdChanged();
    void conversationSkinMismatchChanged();
    void openWindowRequested();

private:
    struct ExpressionSegment
    {
        int segmentId = -1;
        QString state;
        QString expression;
        QString textBuffer;
    };

    struct PendingToolCall
    {
        QString runId;
        QString toolCallId;
        QString toolName;
        QString toolArgs;
    };

    QVariantMap messageObject(const QString &role, const QString &text, bool pending, bool error) const;
    void appendMessage(const QVariantMap &message);
    void transitionTo(ChatPhase next);
    void handleCleanFinishReady();
    void requestCleanFinishForCurrentStream();
    bool canDelayCleanFinishForCurrentAnimation() const;
    void requestGateCleanFinishIfTextDrained();
    void requestFinishCleanFinishIfPacerEmpty();
    void requestCleanFinishForStream(quint64 streamId, quint64 generation, quint64 cleanFinishId);
    void deferCleanFinishRequest(quint64 streamId, quint64 generation, quint64 cleanFinishId);
    void requestDeferredCleanFinishIfPossible();
    void clearDeferredCleanFinishRequest();
    bool runtimeCallbackStillCurrent(quint64 streamId, quint64 generation) const;
    void enqueueExpressionSegment(const QString &state, const QString &expression);
    void appendTextToCurrentTarget(const QString &text);
    void activateNextSegment();
    void enterGateForNextSegment();
    void maybeAdvanceGate();
    void maybeFinishWaitingForAnimationEnd();
    void handleGateTimeout();
    void handleSegmentDrained(int segmentId);
    void handlePacerEmpty();
    void handleStartTimeout();
    void handleToolCall(const ChatStreamEvent &event);
    bool hasPendingToolCall() const;
    void executeToolCallAfterCleanFinish();
    void executePetMotionTool(const PendingToolCall &toolCall);
    bool parsePetMotionArgs(const QString &toolArgs, QString *action, double *x, double *y, QString *mode) const;
    QVariantMap invalidToolResult(const QString &error) const;
    void postToolResult(const QString &runId, const QString &toolCallId, const QVariantMap &result);
    void resumeAfterToolResult();
    void handleMotionToolCompleted(const QVariantMap &result);
    void handleMotionToolInterrupted(const QVariantMap &result);
    void handleMotionToolTimeout();
    void drainQueuedSegmentsToPacer();
    void resetReplySessionState();
    void appendChunkToCurrentMessage(const QString &chunk, quint64 streamId);
    void setSidecarReady(bool ready);
    void setProviderConfigured(bool configured);
    bool updateProviderConfiguredFromSettings();
    QString idleStatusText() const;
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
    void sendMessageInConversation(const QString &trimmed);
    void abortPendingConversationCreate();
    void isolateConversationAsyncState();
    void setCurrentConversationId(const QString &id);
    void setConversationSkinState(const QString &skinId);
    void setConversationSkinMismatch(bool mismatch, const QString &hint);
    void updateConversationCurrentFlags();
    QString currentSkinId() const;

    PetRuntime *m_runtime = nullptr;
    SettingsService *m_settings = nullptr;
    QNetworkAccessManager m_network;
    QProcess m_sidecarProcess;
    QPointer<QNetworkReply> m_currentReply;
    QPointer<QNetworkReply> m_pendingConversationCreateReply;
    ChatStreamEventParser m_parser;
    QVariantList m_messages;
    QVariantList m_conversations;
    QString m_preExpressionBuffer;
    QList<ExpressionSegment> m_segmentQueue;
    ChatPhase m_phase = ChatPhase::IDLE;
    QString m_pendingThinkingState;
    QString m_pendingThinkingExpression;
    PendingToolCall m_pendingToolCall;
    int m_nextSegmentId = 0;
    int m_activeSegmentId = -1;
    QString m_activeState;
    QString m_activeExpression;
    ChatTextPacer *m_pacer = nullptr;
    QTimer m_gateTimeout;
    QTimer m_startTimeout;
    QTimer m_motionToolTimeout;
    static constexpr int kGateTimeoutMs = 2000;
    static constexpr int kStartTimeoutMs = 3000;
    static constexpr int kMotionToolTimeoutMs = 90000;
    bool m_sidecarReady = false;
    bool m_providerConfigured = false;
    bool m_sending = false;
    bool m_sidecarRestartPending = false;
    bool m_sidecarStoppingForRestart = false;
    bool m_foreignSidecarCleanupAttempted = false;
    int m_sidecarRestartAttempts = 0;
    bool m_streamFinished = false;
    bool m_animationReady = false;
    bool m_textDrained = false;
    bool m_pacerEmpty = true;
    bool m_cleanFinishRequestPending = false;
    bool m_motionToolTimedOut = false;
    quint64 m_deferredCleanFinishStreamId = 0;
    quint64 m_deferredCleanFinishGeneration = 0;
    quint64 m_deferredCleanFinishRequestId = 0;
    // 用户取消后，剩余 SSE chunks 必须被丢弃，否则会拼到新建的 assistant 消息里产生"幽灵回复"。
    // 每次 sendMessage 复位 false。
    bool m_cancelled = false;
    QString m_statusText = QStringLiteral("未连接");
    QString m_activeRunId;
    QString m_currentConversationId;
    QString m_currentConversationSkinId;
    QString m_conversationSkinHint;
    int m_assistantMessageIndex = -1;
    quint64 m_currentStreamId = 1;
    quint64 m_asyncGeneration = 1;
    quint64 m_cleanFinishRequestId = 1;
    quint64 m_chatRequestId = 0;
    quint64 m_pendingCreateRequestId = 0;
    quint64 m_listRequestId = 0;
    quint64 m_messageLoadRequestId = 0;
    QSet<quint64> m_completedChatRequestIds;
    bool m_conversationSkinMismatch = false;
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
