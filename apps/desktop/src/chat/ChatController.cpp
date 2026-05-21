#include "chat/ChatController.h"

#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr auto kHealthUrl = "http://127.0.0.1:39710/health";
constexpr auto kChatMessagesUrl = "http://127.0.0.1:39710/v1/chat/messages";
constexpr auto kExpressionRequestedEvent = "miles.pet.expression.requested";

InterruptHint interruptHintFromValue(const QVariantMap &value)
{
    if (value.value(QStringLiteral("interruptHint")).toString() == QStringLiteral("afterCurrent")) {
        return InterruptHint::AfterCurrent;
    }

    return InterruptHint::Immediate;
}
} // namespace

ChatController::ChatController(PetRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
{
    connect(&m_sidecarProcess, &QProcess::started, this, [this]() {
        setStatusText(QStringLiteral("连接中"));
        QTimer::singleShot(250, this, &ChatController::checkHealth);
    });
    connect(&m_sidecarProcess, &QProcess::errorOccurred, this, [this]() {
        setSidecarReady(false);
        setStatusText(QStringLiteral("未连接"));
    });
    connect(&m_sidecarProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) {
                setSidecarReady(false);
                setStatusText(QStringLiteral("未连接"));
            });
}

ChatController::~ChatController()
{
    cancelCurrentReply();
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        m_sidecarProcess.terminate();
        if (!m_sidecarProcess.waitForFinished(1000)) {
            m_sidecarProcess.kill();
            m_sidecarProcess.waitForFinished(1000);
        }
    }
}

void ChatController::openWindow()
{
    emit openWindowRequested();
}

void ChatController::startSidecar()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        checkHealth();
        return;
    }

    const QString executablePath = sidecarExecutablePath();
    if (executablePath.isEmpty()) {
        setSidecarReady(false);
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    setStatusText(QStringLiteral("启动中"));
    m_sidecarProcess.start(executablePath, {QStringLiteral("-addr"), QStringLiteral("127.0.0.1:39710")});
}

void ChatController::checkHealth()
{
    QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(QString::fromLatin1(kHealthUrl))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool healthy = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        setSidecarReady(healthy);
        setStatusText(healthy ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    });
}

void ChatController::sendMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty() || m_sending) {
        return;
    }

    m_cancelled = false;
    appendMessage(messageObject(QStringLiteral("user"), trimmed, false, false));
    appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
    m_assistantMessageIndex = m_messages.size() - 1;

    setSending(true);
    setStatusText(QStringLiteral("正在回复"));
    requestPetExpression(QStringLiteral("thinking"), QStringLiteral("neutral"));

    QJsonObject body;
    body.insert(QStringLiteral("conversationId"), QStringLiteral("default"));
    body.insert(QStringLiteral("message"), trimmed);

    if (QCoreApplication::instance() == nullptr) {
        return;
    }

    QNetworkRequest request(QUrl(QString::fromLatin1(kChatMessagesUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "text/event-stream");

    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_currentReply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (reply == m_currentReply) {
            handleStreamBytes(reply->readAll());
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_currentReply) {
            reply->deleteLater();
            return;
        }

        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        m_currentReply.clear();
        reply->deleteLater();

        if (m_cancelled) {
            return;
        }

        if (error != QNetworkReply::NoError) {
            failCurrentReply(errorString);
            return;
        }

        finishCurrentReply();
    });
}

void ChatController::cancelCurrentReply()
{
    if (!m_sending && m_currentReply.isNull()) {
        return;
    }

    m_cancelled = true;
    if (m_currentReply) {
        QNetworkReply *reply = m_currentReply;
        m_currentReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    m_assistantMessageIndex = -1;
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    requestPetExpression(QStringLiteral("idle"), QStringLiteral("neutral"));
}

void ChatController::applyStreamEvent(const ChatStreamEvent &event)
{
    if (m_cancelled) {
        return;
    }

    if (event.type == QStringLiteral("RUN_STARTED")) {
        setSending(true);
        setStatusText(QStringLiteral("正在回复"));
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_START")) {
        if (event.role == QStringLiteral("assistant")) {
            if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
                appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
                m_assistantMessageIndex = m_messages.size() - 1;
            }
        }
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_CONTENT")) {
        appendAssistantDelta(event.delta);
        return;
    }

    if (event.type == QStringLiteral("TEXT_MESSAGE_END")) {
        if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
            QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
            message.insert(QStringLiteral("pending"), false);
            m_messages[m_assistantMessageIndex] = message;
            emit messagesChanged();
        }
        return;
    }

    if (event.type == QStringLiteral("RUN_FINISHED")) {
        finishCurrentReply();
        return;
    }

    if (event.type == QStringLiteral("RUN_ERROR")) {
        failCurrentReply(event.error);
        return;
    }

    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kExpressionRequestedEvent)) {
        requestPetExpression(event.value.value(QStringLiteral("state")).toString(),
                             event.value.value(QStringLiteral("expression")).toString(),
                             interruptHintFromValue(event.value));
    }
}

QVariantMap ChatController::messageObject(const QString &role, const QString &text, bool pending, bool error) const
{
    QVariantMap message;
    message.insert(QStringLiteral("role"), role);
    message.insert(QStringLiteral("text"), text);
    message.insert(QStringLiteral("pending"), pending);
    message.insert(QStringLiteral("error"), error);
    return message;
}

void ChatController::appendMessage(const QVariantMap &message)
{
    m_messages.append(message);
    emit messagesChanged();
}

void ChatController::appendAssistantDelta(const QString &delta)
{
    if (m_cancelled) {
        return;
    }

    if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }

    QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
    message.insert(QStringLiteral("text"), message.value(QStringLiteral("text")).toString() + delta);
    message.insert(QStringLiteral("pending"), true);
    m_messages[m_assistantMessageIndex] = message;
    emit messagesChanged();
}

void ChatController::setSidecarReady(bool ready)
{
    if (m_sidecarReady == ready) {
        return;
    }

    m_sidecarReady = ready;
    emit sidecarReadyChanged();
}

void ChatController::setSending(bool sending)
{
    if (m_sending == sending) {
        return;
    }

    m_sending = sending;
    emit sendingChanged();
}

void ChatController::setStatusText(const QString &statusText)
{
    if (m_statusText == statusText) {
        return;
    }

    m_statusText = statusText;
    emit statusTextChanged();
}

void ChatController::requestPetExpression(const QString &state, const QString &expression, InterruptHint interruptHint)
{
    if (m_runtime == nullptr) {
        return;
    }

    const QString nextState = state.trimmed().isEmpty() ? QStringLiteral("idle") : state.trimmed();
    const QString nextExpression = expression.trimmed().isEmpty() ? QStringLiteral("neutral") : expression.trimmed();
    m_runtime->requestExpression(nextState, nextExpression, interruptHint);
}

QString ChatController::sidecarExecutablePath() const
{
    const QByteArray envPath = qgetenv("MILESEDGEWORTH_AGENT_PATH");
    if (!envPath.isEmpty()) {
        const QFileInfo envInfo(QString::fromLocal8Bit(envPath));
        if (envInfo.exists() && envInfo.isFile()) {
            return envInfo.absoluteFilePath();
        }
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
    const QString executableName = QStringLiteral("miles-agent.exe");
#else
    const QString executableName = QStringLiteral("miles-agent");
#endif
    const QFileInfo appInfo(appDir.filePath(executableName));
    if (appInfo.exists() && appInfo.isFile()) {
        return appInfo.absoluteFilePath();
    }

    return QString();
}

void ChatController::handleStreamBytes(const QByteArray &bytes)
{
    const QList<ChatStreamEvent> events = m_parser.ingest(bytes);
    for (const ChatStreamEvent &event : events) {
        applyStreamEvent(event);
    }
}

void ChatController::finishCurrentReply()
{
    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    m_assistantMessageIndex = -1;
    m_currentReply.clear();
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
}

void ChatController::failCurrentReply(const QString &message)
{
    const QString text = message.trimmed().isEmpty() ? QStringLiteral("请求失败") : message.trimmed();

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap assistantMessage = m_messages.at(m_assistantMessageIndex).toMap();
        assistantMessage.insert(QStringLiteral("text"), text);
        assistantMessage.insert(QStringLiteral("pending"), false);
        assistantMessage.insert(QStringLiteral("error"), true);
        m_messages[m_assistantMessageIndex] = assistantMessage;
        emit messagesChanged();
    } else {
        appendMessage(messageObject(QStringLiteral("assistant"), text, false, true));
    }

    m_assistantMessageIndex = -1;
    m_currentReply.clear();
    setSending(false);
    setStatusText(QStringLiteral("错误"));
    requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
}
