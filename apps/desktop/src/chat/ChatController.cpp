#include "chat/ChatController.h"

#include "chat/ChatTextPacer.h"
#include "pet/PetRuntime.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QProcessEnvironment>

namespace {
Q_LOGGING_CATEGORY(chatLog, "miles.chat")

constexpr auto kHealthUrl = "http://127.0.0.1:39710/health";
constexpr auto kChatMessagesUrl = "http://127.0.0.1:39710/v1/chat/messages";
constexpr auto kExpressionRequestedEvent = "miles.pet.expression.requested";
constexpr int kSidecarRestartDelayMs = 150;
constexpr int kSidecarRestartRetryDelayMs = 300;
constexpr int kSidecarRestartMaxAttempts = 3;

InterruptHint interruptHintFromValue(const QVariantMap &value)
{
    if (value.value(QStringLiteral("interruptHint")).toString() == QStringLiteral("afterCurrent")) {
        return InterruptHint::AfterCurrent;
    }

    return InterruptHint::Immediate;
}

void logProcessOutput(const char *label, const QByteArray &bytes)
{
    const QList<QByteArray> lines = bytes.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (!line.trimmed().isEmpty()) {
            qCDebug(chatLog).noquote() << label << QString::fromLocal8Bit(line);
        }
    }
}
} // namespace

ChatController::ChatController(PetRuntime *runtime, SettingsService *settings, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
    , m_settings(settings)
{
    connect(&m_sidecarProcess, &QProcess::started, this, [this]() {
        setStatusText(QStringLiteral("连接中"));
        qCDebug(chatLog) << "sidecar process started"
                         << "pid=" << m_sidecarProcess.processId()
                         << "program=" << m_sidecarProcess.program();
        QTimer::singleShot(250, this, &ChatController::checkHealth);
    });
    connect(&m_sidecarProcess, &QProcess::errorOccurred, this, [this]() {
        qCDebug(chatLog) << "sidecar process error"
                         << "error=" << m_sidecarProcess.error()
                         << "message=" << m_sidecarProcess.errorString();
        setSidecarReady(false);
        if (!m_sidecarRestartPending) {
            setStatusText(QStringLiteral("未连接"));
        }
    });
    connect(&m_sidecarProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) {
                qCDebug(chatLog) << "sidecar process finished"
                                 << "exitCode=" << m_sidecarProcess.exitCode()
                                 << "exitStatus=" << m_sidecarProcess.exitStatus();
                setSidecarReady(false);
                if (m_sidecarStoppingForRestart) {
                    setStatusText(QStringLiteral("重启中"));
                    return;
                }
                if (m_sidecarRestartPending && m_sidecarRestartAttempts < kSidecarRestartMaxAttempts) {
                    setStatusText(QStringLiteral("重试中"));
                    scheduleSidecarStart(kSidecarRestartRetryDelayMs);
                    return;
                }
                m_sidecarRestartPending = false;
                m_sidecarRestartAttempts = 0;
                setStatusText(QStringLiteral("未连接"));
            });
    connect(&m_sidecarProcess, &QProcess::readyReadStandardError, this, [this]() {
        logProcessOutput("sidecar stderr", m_sidecarProcess.readAllStandardError());
    });
    connect(&m_sidecarProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        logProcessOutput("sidecar stdout", m_sidecarProcess.readAllStandardOutput());
    });

    m_pacer = new ChatTextPacer(this);
    connect(m_pacer, &ChatTextPacer::chunkReady,
            this, &ChatController::appendChunkToCurrentMessage);
    if (m_settings != nullptr) {
        m_pacer->setMsPerChar(m_settings->msPerChar());
    }

    m_gateTimeout.setSingleShot(true);
    connect(&m_gateTimeout, &QTimer::timeout,
            this, &ChatController::handleGateTimeout);
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
    m_sidecarRestartPending = false;
    m_sidecarRestartAttempts = 0;
    launchSidecarProcess();
}

void ChatController::launchSidecarProcess()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        qCDebug(chatLog) << "sidecar already running under this app"
                         << "pid=" << m_sidecarProcess.processId();
        checkHealth();
        return;
    }

    const QString executablePath = sidecarExecutablePath();
    if (executablePath.isEmpty()) {
        m_sidecarRestartPending = false;
        m_sidecarRestartAttempts = 0;
        setSidecarReady(false);
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_settings != nullptr) {
        const QString baseUrl = m_settings->baseUrl();
        if (!baseUrl.isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_BASE_URL"), baseUrl);
        }

        const QString apiKey = m_settings->apiKey();
        if (!apiKey.isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_API_KEY"), apiKey);
        }

        const QString model = m_settings->model();
        if (!model.isEmpty()) {
            env.insert(QStringLiteral("MILES_PROVIDER_MODEL"), model);
        }

        env.insert(QStringLiteral("MILES_PROVIDER_TEMPERATURE"), QString::number(m_settings->temperature()));
        env.insert(QStringLiteral("MILES_PROVIDER_MAX_TOKENS"), QString::number(m_settings->maxTokens()));
    }

    m_sidecarProcess.setProcessEnvironment(env);
    qCDebug(chatLog) << "launch sidecar"
                     << "path=" << executablePath
                     << "baseUrlSet=" << env.contains(QStringLiteral("MILES_PROVIDER_BASE_URL"))
                     << "apiKeySet=" << env.contains(QStringLiteral("MILES_PROVIDER_API_KEY"))
                     << "model=" << env.value(QStringLiteral("MILES_PROVIDER_MODEL"))
                     << "debug=" << env.contains(QStringLiteral("MILES_DEBUG_CHAT"));
    setStatusText(QStringLiteral("启动中"));
    if (m_sidecarRestartPending) {
        ++m_sidecarRestartAttempts;
    }
    m_sidecarProcess.start(executablePath, {QStringLiteral("-addr"), QStringLiteral("127.0.0.1:39710")});
}

void ChatController::scheduleSidecarStart(int delayMs)
{
    QTimer::singleShot(delayMs, this, [this]() {
        if (m_sidecarRestartPending) {
            launchSidecarProcess();
        }
    });
}

void ChatController::restartSidecar()
{
    cancelCurrentReply();
    setSidecarReady(false);
    setStatusText(QStringLiteral("重启中"));
    m_sidecarRestartPending = true;
    m_sidecarRestartAttempts = 0;

    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        m_sidecarStoppingForRestart = true;
        m_sidecarProcess.terminate();
        if (!m_sidecarProcess.waitForFinished(1000)) {
            m_sidecarProcess.kill();
            m_sidecarProcess.waitForFinished(1000);
        }
        m_sidecarStoppingForRestart = false;
    }

    scheduleSidecarStart(kSidecarRestartDelayMs);
}

void ChatController::handleSettingsSaved()
{
    restartSidecar();
    if (m_settings != nullptr && m_pacer != nullptr) {
        m_pacer->setMsPerChar(m_settings->msPerChar());
    }
}

void ChatController::checkHealth()
{
    QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(QString::fromLatin1(kHealthUrl))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const QJsonObject health = QJsonDocument::fromJson(body).object();
        const qint64 ownedPid = m_sidecarProcess.processId();
        const int healthPid = health.value(QStringLiteral("pid")).toInt();
        const bool healthy = reply->error() == QNetworkReply::NoError
            && ownedPid > 0
            && healthPid == ownedPid;
        qCDebug(chatLog) << "sidecar health"
                         << "healthy=" << healthy
                         << "pid=" << healthPid
                         << "ownedPid=" << ownedPid
                         << "provider=" << health.value(QStringLiteral("provider")).toString()
                         << "error=" << reply->errorString();
        reply->deleteLater();
        setSidecarReady(healthy);
        if (healthy) {
            m_sidecarRestartPending = false;
            m_sidecarRestartAttempts = 0;
        }
        setStatusText(healthy ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    });
}

void ChatController::sendMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty() || m_sending) {
        return;
    }

    ++m_currentStreamId;
    if (m_pacer != nullptr) {
        m_pacer->discardBeforeStream(m_currentStreamId);
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
    if (m_runtime != nullptr) {
        QJsonArray expressionsArray;
        const auto &manifestExpressions = m_runtime->manifest().expressions;
        for (auto it = manifestExpressions.constBegin(); it != manifestExpressions.constEnd(); ++it) {
            const auto &def = it.value();
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), def.id);
            if (!def.label.isEmpty()) {
                entry.insert(QStringLiteral("label"), def.label);
            }
            if (!def.description.isEmpty()) {
                entry.insert(QStringLiteral("description"), def.description);
            }
            if (!def.allowedStates.isEmpty()) {
                QJsonArray allowedStates;
                for (const QString &state : def.allowedStates) {
                    allowedStates.append(state);
                }
                entry.insert(QStringLiteral("allowedStates"), allowedStates);
            }
            expressionsArray.append(entry);
        }
        body.insert(QStringLiteral("expressions"), expressionsArray);
        qCDebug(chatLog) << "send chat request"
                         << "messageLen=" << trimmed.size()
                         << "expressions=" << expressionsArray.size();
    }

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
    const bool activePhase = m_phase != ChatPhase::IDLE;
    if (!m_sending && m_currentReply.isNull() && !activePhase) {
        return;
    }

    m_cancelled = true;
    m_gateTimeout.stop();
    m_pendingExpression.clear();
    m_pendingState.clear();
    if (!m_holdBuffer.isEmpty()
            && (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size())) {
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }
    drainHoldBufferToPacer();
    transitionTo(ChatPhase::IDLE);
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

    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    if (m_runtime != nullptr) {
        m_runtime->returnToIdle();
    }
}

void ChatController::applyStreamEvent(const ChatStreamEvent &event)
{
    if (m_cancelled) {
        return;
    }

    if (event.type == QStringLiteral("RUN_STARTED")) {
        ++m_currentStreamId;
        if (m_pacer != nullptr) {
            m_pacer->discardBeforeStream(m_currentStreamId);
        }
        const bool wasSending = m_sending;
        if (!wasSending) {
            m_assistantMessageIndex = -1;
        }
        setSending(true);
        setStatusText(QStringLiteral("正在回复"));
        m_holdBuffer.clear();
        m_pendingExpression.clear();
        m_pendingState.clear();
        transitionTo(ChatPhase::BUFFERING_FOR_START);
        if (m_runtime != nullptr) {
            QPointer<ChatController> self(this);
            m_runtime->requestCleanFinishAndNotify([self]() {
                if (self != nullptr) {
                    self->handleCleanFinishReady();
                }
            });
        } else {
            handleCleanFinishReady();
        }
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
        if (m_phase == ChatPhase::STREAMING) {
            if (m_pacer != nullptr) {
                m_pacer->append(event.delta, m_currentStreamId);
            } else {
                appendChunkToCurrentMessage(event.delta, m_currentStreamId);
            }
        } else if (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
            if (m_pacer != nullptr) {
                m_pacer->append(event.delta, m_currentStreamId);
            } else {
                appendChunkToCurrentMessage(event.delta, m_currentStreamId);
            }
        } else {
            m_holdBuffer.append(event.delta);
        }
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
        if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
            QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
            message.insert(QStringLiteral("pending"), false);
            m_messages[m_assistantMessageIndex] = message;
            emit messagesChanged();
        }

        m_currentReply.clear();
        m_pendingExpression.clear();
        m_pendingState.clear();
        setSending(false);
        setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
        drainHoldBufferToPacer();

        if (m_runtime == nullptr) {
            transitionTo(ChatPhase::IDLE);
            m_assistantMessageIndex = -1;
            return;
        }

        transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
        QPointer<ChatController> self(this);
        m_runtime->requestBoundaryAndNotify([self]() {
            if (self != nullptr) {
                self->handleBoundaryReached();
            }
        });
        return;
    }

    if (event.type == QStringLiteral("RUN_ERROR")) {
        failCurrentReply(event.error);
        return;
    }

    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kExpressionRequestedEvent)) {
        const QString state = event.value.value(QStringLiteral("state")).toString();
        const QString expression = event.value.value(QStringLiteral("expression")).toString();
        qCDebug(chatLog).noquote() << "chat expression requested"
                                    << "phase=" << static_cast<int>(m_phase)
                                    << "state=" << state
                                    << "expression=" << expression
                                    << "interruptHint=" << event.value.value(QStringLiteral("interruptHint")).toString();

        if (m_phase == ChatPhase::BUFFERING_FOR_START || m_phase == ChatPhase::GATED) {
            m_pendingState = state;
            m_pendingExpression = expression;
            return;
        }

        if (m_phase == ChatPhase::STREAMING) {
            m_pendingState = state;
            m_pendingExpression = expression;
            transitionTo(ChatPhase::GATED);
            m_gateTimeout.start(kGateTimeoutMs);
            if (m_runtime != nullptr) {
                QPointer<ChatController> self(this);
                m_runtime->requestBoundaryAndNotify([self]() {
                    if (self != nullptr) {
                        self->handleBoundaryReached();
                    }
                });
            }
            return;
        }

        requestPetExpression(state, expression, interruptHintFromValue(event.value));
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

void ChatController::transitionTo(ChatPhase next)
{
    if (m_phase == next) {
        return;
    }
    m_phase = next;
}

void ChatController::handleCleanFinishReady()
{
    if (m_phase != ChatPhase::BUFFERING_FOR_START) {
        return;
    }

    if (!m_pendingExpression.isEmpty()) {
        requestPetExpression(m_pendingState, m_pendingExpression);
        m_pendingExpression.clear();
        m_pendingState.clear();
    }
    transitionTo(ChatPhase::STREAMING);
    drainHoldBufferToPacer();
}

void ChatController::handleBoundaryReached()
{
    m_gateTimeout.stop();
    if (m_phase == ChatPhase::GATED) {
        if (!m_pendingExpression.isEmpty()) {
            requestPetExpression(m_pendingState, m_pendingExpression);
            m_pendingExpression.clear();
            m_pendingState.clear();
        }
        transitionTo(ChatPhase::STREAMING);
        drainHoldBufferToPacer();
        return;
    }

    if (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        if (m_runtime != nullptr) {
            m_runtime->returnToIdle();
        }
        transitionTo(ChatPhase::IDLE);
    }
}

void ChatController::handleGateTimeout()
{
    if (m_phase != ChatPhase::GATED) {
        return;
    }

    if (!m_pendingExpression.isEmpty()) {
        requestPetExpression(m_pendingState, m_pendingExpression);
        m_pendingExpression.clear();
        m_pendingState.clear();
    }
    transitionTo(ChatPhase::STREAMING);
    drainHoldBufferToPacer();
}

void ChatController::drainHoldBufferToPacer()
{
    if (m_holdBuffer.isEmpty()) {
        return;
    }

    if (m_pacer != nullptr) {
        m_pacer->append(m_holdBuffer, m_currentStreamId);
    } else {
        appendChunkToCurrentMessage(m_holdBuffer, m_currentStreamId);
    }
    m_holdBuffer.clear();
}

void ChatController::appendChunkToCurrentMessage(const QString &chunk, quint64 streamId)
{
    if (chunk.isEmpty() || streamId != m_currentStreamId) {
        return;
    }

    if (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size()) {
        if (m_cancelled) {
            return;
        }
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }

    QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
    message.insert(QStringLiteral("text"), message.value(QStringLiteral("text")).toString() + chunk);
    message.insert(QStringLiteral("pending"), m_sending);
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
    qCDebug(chatLog) << "stream bytes received"
                     << "bytes=" << bytes.size()
                     << "events=" << events.size();
    for (const ChatStreamEvent &event : events) {
        qCDebug(chatLog) << "stream event"
                         << "type=" << event.type
                         << "name=" << event.name
                         << "state=" << event.value.value(QStringLiteral("state")).toString()
                         << "expression=" << event.value.value(QStringLiteral("expression")).toString()
                         << "deltaLen=" << event.delta.size();
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
    m_holdBuffer.clear();
    m_currentReply.clear();
    setSending(false);
    setStatusText(m_sidecarReady ? QStringLiteral("已连接") : QStringLiteral("未连接"));
}

void ChatController::failCurrentReply(const QString &message)
{
    const QString text = message.trimmed().isEmpty() ? QStringLiteral("请求失败") : message.trimmed();
    const bool hasBufferedText = !m_holdBuffer.isEmpty();

    m_gateTimeout.stop();
    m_pendingExpression.clear();
    m_pendingState.clear();
    if (hasBufferedText
            && (m_assistantMessageIndex < 0 || m_assistantMessageIndex >= m_messages.size())) {
        appendMessage(messageObject(QStringLiteral("assistant"), QString(), true, false));
        m_assistantMessageIndex = m_messages.size() - 1;
    }
    drainHoldBufferToPacer();
    transitionTo(ChatPhase::IDLE);

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap assistantMessage = m_messages.at(m_assistantMessageIndex).toMap();
        if (assistantMessage.value(QStringLiteral("text")).toString().isEmpty() && !hasBufferedText) {
            assistantMessage.insert(QStringLiteral("text"), text);
        }
        assistantMessage.insert(QStringLiteral("pending"), false);
        assistantMessage.insert(QStringLiteral("error"), true);
        m_messages[m_assistantMessageIndex] = assistantMessage;
        emit messagesChanged();
    } else {
        appendMessage(messageObject(QStringLiteral("assistant"), text, false, true));
    }

    m_currentReply.clear();
    setSending(false);
    setStatusText(QStringLiteral("错误"));
    requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
}
