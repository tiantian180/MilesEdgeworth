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
    void appendAssistantDelta(const QString &delta);
    void flushHoldBuffer();
    void setSidecarReady(bool ready);
    void setSending(bool sending);
    void setStatusText(const QString &statusText);
    void requestPetExpression(
        const QString &state,
        const QString &expression,
        InterruptHint interruptHint = InterruptHint::Immediate
    );
    QString sidecarExecutablePath() const;
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
    // Phase 2.1 plumbing: accumulate streamed deltas before pushing to m_messages.
    // Phase 2.3 will gate this buffer on PetRuntime animation boundaries; for now
    // every Feed flushes immediately, matching the previous direct-append behavior.
    QString m_holdBuffer;
    bool m_sidecarReady = false;
    bool m_sending = false;
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
