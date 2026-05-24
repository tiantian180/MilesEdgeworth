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
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#endif

namespace {
Q_LOGGING_CATEGORY(chatLog, "miles.chat", QtInfoMsg)

constexpr auto kHealthUrl = "http://127.0.0.1:39710/health";
constexpr auto kConversationsUrl = "http://127.0.0.1:39710/v1/conversations";
constexpr auto kChatMessagesUrl = "http://127.0.0.1:39710/v1/chat/messages";
constexpr auto kExpressionRequestedEvent = "miles.pet.expression.requested";
constexpr auto kMemorySummarizingEvent = "miles.chat.memory.summarizing";
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

QString logBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

bool terminateForeignSidecar(qint64 pid, const QString &service)
{
    if (service != QStringLiteral("miles-agent") || pid <= 0 || pid == QCoreApplication::applicationPid()) {
        return false;
    }

#ifdef Q_OS_UNIX
    if (::kill(static_cast<pid_t>(pid), SIGTERM) == 0) {
        return true;
    }
    return errno == ESRCH;
#else
    Q_UNUSED(pid)
    return false;
#endif
}

} // namespace

ChatController::ChatController(PetRuntime *runtime, SettingsService *settings, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
    , m_settings(settings)
{
    m_sidecarProcess.setProcessChannelMode(QProcess::ForwardedErrorChannel);

    connect(&m_sidecarProcess, &QProcess::started, this, [this]() {
        setStatusText(QStringLiteral("连接中"));
        qCDebug(chatLog).noquote() << "sidecar process started"
                                    << QStringLiteral("pid=%1").arg(m_sidecarProcess.processId())
                                    << QStringLiteral("program=%1").arg(m_sidecarProcess.program());
        QTimer::singleShot(250, this, &ChatController::checkHealth);
    });
    connect(&m_sidecarProcess, &QProcess::errorOccurred, this, [this]() {
        qCDebug(chatLog).noquote() << "sidecar process error"
                                    << QStringLiteral("error=%1").arg(m_sidecarProcess.error())
                                    << QStringLiteral("message=%1").arg(m_sidecarProcess.errorString());
        setSidecarReady(false);
        if (!m_sidecarRestartPending) {
            setStatusText(QStringLiteral("未连接"));
        }
    });
    connect(&m_sidecarProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) {
                qCDebug(chatLog).noquote() << "sidecar process finished"
                                            << QStringLiteral("exitCode=%1").arg(m_sidecarProcess.exitCode())
                                            << QStringLiteral("exitStatus=%1").arg(m_sidecarProcess.exitStatus());
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

    m_pacer = new ChatTextPacer(this);
    connect(m_pacer, &ChatTextPacer::chunkReady,
            this, &ChatController::appendChunkToCurrentMessage);
    if (m_settings != nullptr) {
        m_pacer->setMsPerChar(m_settings->msPerChar());
        updateProviderConfiguredFromSettings();
    }

    m_gateTimeout.setSingleShot(true);
    connect(&m_gateTimeout, &QTimer::timeout,
            this, &ChatController::handleGateTimeout);

    if (m_runtime != nullptr) {
        connect(m_runtime, &PetRuntime::activeSkinChanged, this, [this]() {
            setConversationSkinState(m_currentConversationSkinId);
        });
        connect(m_runtime, &PetRuntime::skinManifestReloaded, this, [this]() {
            setConversationSkinState(m_currentConversationSkinId);
        });
    }
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
    m_foreignSidecarCleanupAttempted = false;
    launchSidecarProcess();
}

void ChatController::launchSidecarProcess()
{
    if (m_sidecarProcess.state() != QProcess::NotRunning) {
        qCDebug(chatLog).noquote() << "sidecar already running under this app"
                                    << QStringLiteral("pid=%1").arg(m_sidecarProcess.processId());
        checkHealth();
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty()) {
        QDir().mkpath(dataDir);
        env.insert(QStringLiteral("MILES_DATA_DIR"), dataDir);
    }

    bool providerConfigured = false;
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

        providerConfigured = m_settings->providerConfigured();
        const auto temperature = m_settings->temperature();
        if (temperature.has_value()) {
            env.insert(QStringLiteral("MILES_PROVIDER_TEMPERATURE"), QString::number(*temperature));
        }
        const auto maxTokens = m_settings->maxTokens();
        if (maxTokens.has_value()) {
            env.insert(QStringLiteral("MILES_PROVIDER_MAX_TOKENS"), QString::number(*maxTokens));
        }

        const auto langfuse = m_settings->langfuseConfig();
        if (langfuse.enabled
            && !langfuse.host.isEmpty()
            && !langfuse.publicKey.isEmpty()
            && !langfuse.secretKey.isEmpty()) {
            env.insert(QStringLiteral("MILES_LANGFUSE_ENABLED"), QStringLiteral("1"));
            env.insert(QStringLiteral("LANGFUSE_HOST"), langfuse.host);
            env.insert(QStringLiteral("LANGFUSE_PUBLIC_KEY"), langfuse.publicKey);
            env.insert(QStringLiteral("LANGFUSE_SECRET_KEY"), langfuse.secretKey);
            env.insert(QStringLiteral("MILES_LANGFUSE_CAPTURE_CONTENT"),
                       langfuse.captureContent ? QStringLiteral("1") : QStringLiteral("0"));
        }
    }
    setProviderConfigured(providerConfigured);

    const QString executablePath = sidecarExecutablePath();
    if (executablePath.isEmpty()) {
        m_sidecarRestartPending = false;
        m_sidecarRestartAttempts = 0;
        setSidecarReady(false);
        setStatusText(QStringLiteral("找不到 miles-agent"));
        return;
    }

    m_sidecarProcess.setProcessEnvironment(env);
    qCDebug(chatLog).noquote() << "launch sidecar"
                               << QStringLiteral("path=%1").arg(executablePath)
                               << QStringLiteral("baseUrlSet=%1").arg(logBool(env.contains(QStringLiteral("MILES_PROVIDER_BASE_URL"))))
                               << QStringLiteral("apiKeySet=%1").arg(logBool(env.contains(QStringLiteral("MILES_PROVIDER_API_KEY"))))
                               << QStringLiteral("model=%1").arg(env.value(QStringLiteral("MILES_PROVIDER_MODEL")))
                               << QStringLiteral("langfuseSet=%1").arg(logBool(env.contains(QStringLiteral("LANGFUSE_HOST"))))
                               << QStringLiteral("langfuseCaptureContent=%1").arg(logBool(env.value(QStringLiteral("MILES_LANGFUSE_CAPTURE_CONTENT")) == QStringLiteral("1")))
                               << QStringLiteral("logLevel=%1").arg(env.value(QStringLiteral("MILES_LOG_LEVEL"), QStringLiteral("info")))
                               << QStringLiteral("logFileSet=%1").arg(logBool(env.contains(QStringLiteral("MILES_LOG_FILE"))))
                               << QStringLiteral("dataDirSet=%1").arg(logBool(env.contains(QStringLiteral("MILES_DATA_DIR"))))
                               << QStringLiteral("payloads=%1").arg(logBool(env.value(QStringLiteral("MILES_LOG_PAYLOADS")) == QStringLiteral("1")));
    setStatusText(QStringLiteral("启动中"));
    if (m_sidecarRestartPending) {
        ++m_sidecarRestartAttempts;
    }
    // sidecar 需要知道父进程 PID；即使开发期主 app 被调试器强杀，
    // sidecar 也能自行退出，避免残留进程长期占用 39710 端口。
    m_sidecarProcess.start(
        executablePath,
        {
            QStringLiteral("-addr"),
            QStringLiteral("127.0.0.1:39710"),
            QStringLiteral("-parent-pid"),
            QString::number(QCoreApplication::applicationPid()),
        }
    );
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
    m_foreignSidecarCleanupAttempted = false;

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
    updateProviderConfiguredFromSettings();
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
        const QString service = health.value(QStringLiteral("service")).toString();
        const bool healthy = reply->error() == QNetworkReply::NoError
            && ownedPid > 0
            && healthPid == ownedPid;
        qCDebug(chatLog).noquote() << "sidecar health"
                                    << QStringLiteral("healthy=%1").arg(logBool(healthy))
                                    << QStringLiteral("pid=%1").arg(healthPid)
                                    << QStringLiteral("ownedPid=%1").arg(ownedPid)
                                    << QStringLiteral("service=%1").arg(service)
                                    << QStringLiteral("provider=%1").arg(health.value(QStringLiteral("provider")).toString())
                                    << QStringLiteral("error=%1").arg(reply->errorString());
        if (!healthy
                && reply->error() == QNetworkReply::NoError
                && service == QStringLiteral("miles-agent")
                && healthPid > 0
                && healthPid != ownedPid
                && !m_foreignSidecarCleanupAttempted) {
            m_foreignSidecarCleanupAttempted = true;
            if (terminateForeignSidecar(healthPid, service)) {
                qCDebug(chatLog).noquote() << "terminated foreign sidecar"
                                            << QStringLiteral("pid=%1").arg(healthPid);
                reply->deleteLater();
                setSidecarReady(false);
                setStatusText(QStringLiteral("重启中"));
                scheduleSidecarStart(kSidecarRestartRetryDelayMs);
                return;
            }
        }
        reply->deleteLater();
        setSidecarReady(healthy);
        if (healthy) {
            m_foreignSidecarCleanupAttempted = false;
            m_sidecarRestartPending = false;
            m_sidecarRestartAttempts = 0;
            loadConversations();
        }
        setStatusText(healthy ? idleStatusText() : QStringLiteral("未连接"));
    });
}

void ChatController::loadConversations()
{
    const quint64 requestId = ++m_listRequestId;
    QUrl url(QString::fromLatin1(kConversationsUrl));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    url.setQuery(query);

    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        if (requestId != m_listRequestId) {
            reply->deleteLater();
            return;
        }

        const QByteArray body = reply->readAll();
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            qCDebug(chatLog).noquote() << "load conversations failed"
                                        << QStringLiteral("error=%1").arg(errorString);
            return;
        }

        const QJsonArray items = QJsonDocument::fromJson(body).array();
        QVariantList conversations;
        QString firstId;
        QString currentSkinIdFromList;
        for (const QJsonValue &value : items) {
            const QJsonObject object = value.toObject();
            const QString id = object.value(QStringLiteral("id")).toString();
            if (id.isEmpty()) {
                continue;
            }

            QVariantMap conversation;
            conversation.insert(QStringLiteral("id"), id);
            conversation.insert(QStringLiteral("title"), object.value(QStringLiteral("title")).toString());
            conversation.insert(QStringLiteral("skinId"), object.value(QStringLiteral("skinId")).toString());
            conversation.insert(QStringLiteral("updatedAt"), object.value(QStringLiteral("updatedAt")).toString());
            conversation.insert(QStringLiteral("isCurrent"), id == m_currentConversationId);
            conversations.append(conversation);

            if (firstId.isEmpty()) {
                firstId = id;
            }
            if (id == m_currentConversationId) {
                currentSkinIdFromList = object.value(QStringLiteral("skinId")).toString();
            }
        }

        m_conversations = conversations;
        emit conversationsChanged();

        if (m_currentConversationId.isEmpty() && !firstId.isEmpty()) {
            switchConversation(firstId);
            return;
        }

        if (!currentSkinIdFromList.isEmpty()) {
            setConversationSkinState(currentSkinIdFromList);
        }
    });
}

void ChatController::switchConversation(const QString &id)
{
    const QString trimmedId = id.trimmed();
    if (trimmedId.isEmpty()) {
        newConversation();
        return;
    }

    isolateConversationAsyncState();

    QString skinId;
    for (const QVariant &item : m_conversations) {
        const QVariantMap conversation = item.toMap();
        if (conversation.value(QStringLiteral("id")).toString() == trimmedId) {
            skinId = conversation.value(QStringLiteral("skinId")).toString();
            break;
        }
    }

    setCurrentConversationId(trimmedId);
    setConversationSkinState(skinId);
    updateConversationCurrentFlags();

    m_messages.clear();
    m_assistantMessageIndex = -1;
    emit messagesChanged();

    const quint64 requestId = ++m_messageLoadRequestId;
    const QString encodedId = QString::fromUtf8(QUrl::toPercentEncoding(trimmedId));
    QNetworkReply *reply = m_network.get(QNetworkRequest(
        QUrl(QString::fromLatin1(kConversationsUrl) + QStringLiteral("/") + encodedId + QStringLiteral("/messages"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmedId, requestId]() {
        if (requestId != m_messageLoadRequestId) {
            reply->deleteLater();
            return;
        }

        const QByteArray body = reply->readAll();
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();

        if (trimmedId != m_currentConversationId) {
            return;
        }

        if (error != QNetworkReply::NoError) {
            qCDebug(chatLog).noquote() << "load conversation messages failed"
                                        << QStringLiteral("conversationId=%1").arg(trimmedId)
                                        << QStringLiteral("error=%1").arg(errorString);
            return;
        }

        const QJsonArray items = QJsonDocument::fromJson(body).array();
        QVariantList messages;
        for (const QJsonValue &value : items) {
            const QJsonObject object = value.toObject();
            const QString role = object.value(QStringLiteral("role")).toString();
            if (role == QStringLiteral("summary")) {
                continue;
            }

            QVariantMap message;
            message.insert(QStringLiteral("role"), role);
            message.insert(QStringLiteral("text"), object.value(QStringLiteral("content")).toString());
            message.insert(QStringLiteral("pending"), false);
            message.insert(QStringLiteral("error"), false);
            message.insert(QStringLiteral("isPartial"), object.value(QStringLiteral("isPartial")).toBool());
            messages.append(message);
        }

        m_messages = messages;
        m_assistantMessageIndex = -1;
        emit messagesChanged();
    });
}

void ChatController::newConversation()
{
    isolateConversationAsyncState();
    setCurrentConversationId(QString());
    m_currentConversationSkinId.clear();
    setConversationSkinMismatch(false, QString());
    updateConversationCurrentFlags();

    m_messages.clear();
    m_assistantMessageIndex = -1;
    emit messagesChanged();
}

void ChatController::deleteConversation(const QString &id)
{
    const QString trimmedId = id.trimmed();
    if (trimmedId.isEmpty()) {
        return;
    }

    const bool deletingCurrent = trimmedId == m_currentConversationId;
    if (deletingCurrent) {
        isolateConversationAsyncState();
    }

    const QString encodedId = QString::fromUtf8(QUrl::toPercentEncoding(trimmedId));
    QNetworkRequest request(QUrl(QString::fromLatin1(kConversationsUrl) + QStringLiteral("/") + encodedId));
    QNetworkReply *reply = m_network.deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmedId, deletingCurrent]() {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            qCDebug(chatLog).noquote() << "delete conversation failed"
                                        << QStringLiteral("conversationId=%1").arg(trimmedId)
                                        << QStringLiteral("error=%1").arg(errorString);
            return;
        }

        if (deletingCurrent) {
            newConversation();
        }
        loadConversations();
    });
}

void ChatController::sendMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty() || m_sending) {
        return;
    }
    if (!m_providerConfigured) {
        setStatusText(QStringLiteral("未配置模型"));
        return;
    }

    m_cancelled = false;
    if (m_currentConversationId.isEmpty()) {
        const QString skinId = currentSkinId();
        if (skinId.isEmpty()) {
            setStatusText(QStringLiteral("无法创建会话"));
            return;
        }

        setSending(true);
        setStatusText(QStringLiteral("正在回复"));

        QJsonObject body;
        body.insert(QStringLiteral("skinId"), skinId);

        QNetworkRequest request(QUrl(QString::fromLatin1(kConversationsUrl)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        const quint64 requestId = ++m_pendingCreateRequestId;
        QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        m_pendingConversationCreateReply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, trimmed, requestId]() {
            if (reply != m_pendingConversationCreateReply || requestId != m_pendingCreateRequestId) {
                reply->deleteLater();
                return;
            }

            const QByteArray responseBody = reply->readAll();
            const QNetworkReply::NetworkError error = reply->error();
            const QString errorString = reply->errorString();
            m_pendingConversationCreateReply.clear();
            reply->deleteLater();

            if (m_cancelled) {
                return;
            }

            if (error != QNetworkReply::NoError) {
                setSending(false);
                setStatusText(QStringLiteral("错误"));
                requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
                qCDebug(chatLog).noquote() << "create conversation failed"
                                            << QStringLiteral("error=%1").arg(errorString);
                return;
            }

            const QJsonObject created = QJsonDocument::fromJson(responseBody).object();
            const QString id = created.value(QStringLiteral("id")).toString().trimmed();
            if (id.isEmpty()) {
                setSending(false);
                setStatusText(QStringLiteral("错误"));
                requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
                return;
            }

            setCurrentConversationId(id);
            setConversationSkinState(created.value(QStringLiteral("skinId")).toString());
            loadConversations();
            sendMessageInConversation(trimmed);
        });
        return;
    }

    sendMessageInConversation(trimmed);
}

void ChatController::sendMessageInConversation(const QString &trimmed)
{
    if (trimmed.isEmpty() || m_currentConversationId.isEmpty()) {
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
    body.insert(QStringLiteral("conversationId"), m_currentConversationId);
    body.insert(QStringLiteral("message"), trimmed);
    QJsonArray expressionsArray;
    QString personaPrompt;
    if (m_runtime != nullptr) {
        personaPrompt = m_runtime->manifest().personaPrompt;
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
    }
    body.insert(QStringLiteral("personaPrompt"), personaPrompt);
    body.insert(QStringLiteral("expressions"), expressionsArray);
    qCDebug(chatLog).noquote() << "send chat request"
                                << QStringLiteral("conversationId=%1").arg(m_currentConversationId)
                                << QStringLiteral("messageLen=%1").arg(trimmed.size())
                                << QStringLiteral("personaPromptLen=%1").arg(personaPrompt.size())
                                << QStringLiteral("expressions=%1").arg(expressionsArray.size());

    if (QCoreApplication::instance() == nullptr) {
        return;
    }

    QNetworkRequest request(QUrl(QString::fromLatin1(kChatMessagesUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "text/event-stream");

    const quint64 requestId = ++m_chatRequestId;
    QNetworkReply *reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_currentReply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, requestId]() {
        if (reply == m_currentReply && requestId == m_chatRequestId) {
            handleStreamBytes(reply->readAll());
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const bool activeReply = reply == m_currentReply && requestId == m_chatRequestId;
        const bool completedReply = m_completedChatRequestIds.contains(requestId);
        if (!activeReply && !completedReply) {
            reply->deleteLater();
            return;
        }

        const QNetworkReply::NetworkError error = reply->error();
        const QString errorString = reply->errorString();
        if (activeReply) {
            m_currentReply.clear();
        }
        if (completedReply) {
            m_completedChatRequestIds.remove(requestId);
        }
        reply->deleteLater();

        if (completedReply && !activeReply) {
            loadConversations();
            return;
        }

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
    const bool pendingCreate = !m_pendingConversationCreateReply.isNull();
    if (!m_sending && m_currentReply.isNull() && !pendingCreate && !activePhase) {
        return;
    }

    m_cancelled = true;
    ++m_asyncGeneration;
    abortPendingConversationCreate();
    m_gateTimeout.stop();
    m_pendingExpression.clear();
    m_pendingState.clear();
    m_finishPendingAfterStart = false;
    m_finishPendingAfterGate = false;
    m_activeRunId.clear();
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
    if (m_runtime != nullptr) {
        m_runtime->setSuppressAutoIdle(false);
    }

    if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
        QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
        message.insert(QStringLiteral("pending"), false);
        m_messages[m_assistantMessageIndex] = message;
        emit messagesChanged();
    }

    setSending(false);
    setStatusText(idleStatusText());
    if (m_runtime != nullptr) {
        m_runtime->returnToIdle();
    }
    if (!m_currentConversationId.isEmpty()) {
        loadConversations();
    }
}

void ChatController::applyStreamEvent(const ChatStreamEvent &event)
{
    if (m_cancelled) {
        return;
    }

    if (event.type == QStringLiteral("RUN_STARTED")) {
        const bool duplicateRunStarted = m_sending
            && m_phase == ChatPhase::BUFFERING_FOR_START
            && ((!event.runId.isEmpty() && event.runId == m_activeRunId)
                || (event.runId.isEmpty() && m_activeRunId.isEmpty()));
        if (duplicateRunStarted) {
            setSending(true);
            setStatusText(QStringLiteral("正在回复"));
            return;
        }

        ++m_currentStreamId;
        ++m_asyncGeneration;
        m_activeRunId = event.runId;
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
        m_finishPendingAfterStart = false;
        m_finishPendingAfterGate = false;
        transitionTo(ChatPhase::BUFFERING_FOR_START);
        if (m_runtime != nullptr) {
            m_runtime->setSuppressAutoIdle(true);
        }
        requestCleanFinishForCurrentStream();
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
        m_completedChatRequestIds.insert(m_chatRequestId);
        if (m_assistantMessageIndex >= 0 && m_assistantMessageIndex < m_messages.size()) {
            QVariantMap message = m_messages.at(m_assistantMessageIndex).toMap();
            message.insert(QStringLiteral("pending"), false);
            m_messages[m_assistantMessageIndex] = message;
            emit messagesChanged();
        }

        m_currentReply.clear();
        m_activeRunId.clear();
        if (m_phase == ChatPhase::BUFFERING_FOR_START) {
            m_finishPendingAfterStart = true;
            setSending(false);
            setStatusText(idleStatusText());
            return;
        }

        if (m_phase == ChatPhase::GATED) {
            m_finishPendingAfterGate = true;
            setSending(false);
            setStatusText(idleStatusText());
            return;
        }

        m_pendingExpression.clear();
        m_pendingState.clear();
        m_finishPendingAfterStart = false;
        m_finishPendingAfterGate = false;
        setSending(false);
        setStatusText(idleStatusText());
        drainHoldBufferToPacer();

        if (m_runtime == nullptr) {
            transitionTo(ChatPhase::IDLE);
            m_assistantMessageIndex = -1;
            return;
        }

        transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
        requestBoundaryForCurrentStream();
        return;
    }

    if (event.type == QStringLiteral("RUN_ERROR")) {
        failCurrentReply(event.error);
        return;
    }

    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kMemorySummarizingEvent)) {
        setStatusText(QStringLiteral("整理记忆中..."));
        return;
    }

    if (event.type == QStringLiteral("CUSTOM") && event.name == QString::fromLatin1(kExpressionRequestedEvent)) {
        const QString state = event.value.value(QStringLiteral("state")).toString();
        const QString expression = event.value.value(QStringLiteral("expression")).toString();
        qCDebug(chatLog).noquote() << "chat expression requested"
                                    << QStringLiteral("phase=%1").arg(static_cast<int>(m_phase))
                                    << QStringLiteral("state=%1").arg(state)
                                    << QStringLiteral("expression=%1").arg(expression)
                                    << QStringLiteral("interruptHint=%1").arg(event.value.value(QStringLiteral("interruptHint")).toString());

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
                requestBoundaryForCurrentStream();
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

    if (m_finishPendingAfterStart) {
        m_finishPendingAfterStart = false;
        if (m_runtime == nullptr) {
            transitionTo(ChatPhase::IDLE);
            m_assistantMessageIndex = -1;
            return;
        }

        transitionTo(ChatPhase::WAITING_FOR_ANIMATION_END);
        requestBoundaryForCurrentStream();
    }
}

void ChatController::requestCleanFinishForCurrentStream()
{
    if (m_runtime == nullptr) {
        handleCleanFinishReady();
        return;
    }

    const quint64 streamId = m_currentStreamId;
    const quint64 generation = m_asyncGeneration;
    QPointer<ChatController> self(this);
    m_runtime->requestCleanFinishAndNotify([self, streamId, generation]() {
        if (self != nullptr && self->runtimeCallbackStillCurrent(streamId, generation)) {
            self->handleCleanFinishReady();
        }
    });
}

void ChatController::requestBoundaryForCurrentStream()
{
    if (m_runtime == nullptr) {
        handleBoundaryReached();
        return;
    }

    const quint64 streamId = m_currentStreamId;
    const quint64 generation = m_asyncGeneration;
    const quint64 boundaryId = ++m_boundaryRequestId;
    QPointer<ChatController> self(this);
    m_runtime->requestCleanFinishAndNotify([self, streamId, generation, boundaryId]() {
        if (self != nullptr && self->boundaryCallbackStillCurrent(streamId, generation, boundaryId)) {
            self->handleBoundaryReached();
        }
    });
}

bool ChatController::runtimeCallbackStillCurrent(quint64 streamId, quint64 generation) const
{
    return streamId == m_currentStreamId
        && generation == m_asyncGeneration
        && !m_cancelled;
}

bool ChatController::boundaryCallbackStillCurrent(quint64 streamId, quint64 generation, quint64 boundaryId) const
{
    return runtimeCallbackStillCurrent(streamId, generation)
        && boundaryId == m_boundaryRequestId;
}

void ChatController::handleBoundaryReached()
{
    ++m_boundaryRequestId;
    m_gateTimeout.stop();
    if (m_phase == ChatPhase::GATED) {
        if (!m_pendingExpression.isEmpty()) {
            requestPetExpression(m_pendingState, m_pendingExpression);
            m_pendingExpression.clear();
            m_pendingState.clear();
        }
        if (m_finishPendingAfterGate) {
            m_finishPendingAfterGate = false;
            if (m_runtime != nullptr) {
                m_runtime->setSuppressAutoIdle(false);
            }
            transitionTo(ChatPhase::IDLE);
            return;
        }
        transitionTo(ChatPhase::STREAMING);
        drainHoldBufferToPacer();
        return;
    }

    if (m_phase == ChatPhase::WAITING_FOR_ANIMATION_END) {
        if (m_runtime != nullptr) {
            m_runtime->returnToIdle();
            m_runtime->setSuppressAutoIdle(false);
        }
        transitionTo(ChatPhase::IDLE);
    }
}

void ChatController::handleGateTimeout()
{
    if (m_phase != ChatPhase::GATED) {
        return;
    }

    ++m_boundaryRequestId;
    if (!m_pendingExpression.isEmpty()) {
        requestPetExpression(m_pendingState, m_pendingExpression);
        m_pendingExpression.clear();
        m_pendingState.clear();
    }
    if (m_finishPendingAfterGate) {
        m_finishPendingAfterGate = false;
        if (m_runtime != nullptr) {
            m_runtime->setSuppressAutoIdle(false);
        }
        transitionTo(ChatPhase::IDLE);
        return;
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

void ChatController::setProviderConfigured(bool configured)
{
    if (m_providerConfigured == configured) {
        return;
    }

    m_providerConfigured = configured;
    emit providerConfiguredChanged();
}

bool ChatController::updateProviderConfiguredFromSettings()
{
    if (m_settings == nullptr) {
        setProviderConfigured(false);
        return false;
    }

    const bool configured = m_settings->providerConfigured();
    setProviderConfigured(configured);
    return configured;
}

QString ChatController::idleStatusText() const
{
    if (!m_sidecarReady) {
        return QStringLiteral("未连接");
    }
    return m_providerConfigured ? QStringLiteral("已连接") : QStringLiteral("未配置模型");
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

void ChatController::setCurrentConversationId(const QString &id)
{
    if (m_currentConversationId == id) {
        return;
    }

    m_currentConversationId = id;
    emit currentConversationIdChanged();
}

void ChatController::setConversationSkinState(const QString &skinId)
{
    m_currentConversationSkinId = skinId;

    const QString activeSkinId = currentSkinId();
    const bool mismatch = !m_currentConversationSkinId.isEmpty()
        && !activeSkinId.isEmpty()
        && m_currentConversationSkinId != activeSkinId;
    const QString hint = mismatch
        ? QStringLiteral("此会话始于皮肤：%1").arg(m_currentConversationSkinId)
        : QString();
    setConversationSkinMismatch(mismatch, hint);
}

void ChatController::setConversationSkinMismatch(bool mismatch, const QString &hint)
{
    if (m_conversationSkinMismatch == mismatch && m_conversationSkinHint == hint) {
        return;
    }

    m_conversationSkinMismatch = mismatch;
    m_conversationSkinHint = hint;
    emit conversationSkinMismatchChanged();
}

void ChatController::updateConversationCurrentFlags()
{
    bool changed = false;
    QVariantList updated;
    updated.reserve(m_conversations.size());
    for (const QVariant &item : m_conversations) {
        QVariantMap conversation = item.toMap();
        const bool isCurrent = conversation.value(QStringLiteral("id")).toString() == m_currentConversationId;
        if (conversation.value(QStringLiteral("isCurrent")).toBool() != isCurrent) {
            conversation.insert(QStringLiteral("isCurrent"), isCurrent);
            changed = true;
        }
        updated.append(conversation);
    }

    if (!changed) {
        return;
    }

    m_conversations = updated;
    emit conversationsChanged();
}

QString ChatController::currentSkinId() const
{
    if (m_runtime == nullptr) {
        return QString();
    }

    return m_runtime->manifest().skinId;
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
    qCDebug(chatLog).noquote() << "stream bytes received"
                                << QStringLiteral("bytes=%1").arg(bytes.size())
                                << QStringLiteral("events=%1").arg(events.size());
    for (const ChatStreamEvent &event : events) {
        qCDebug(chatLog).noquote() << "stream event"
                                    << QStringLiteral("type=%1").arg(event.type)
                                    << QStringLiteral("name=%1").arg(event.name)
                                    << QStringLiteral("state=%1").arg(event.value.value(QStringLiteral("state")).toString())
                                    << QStringLiteral("expression=%1").arg(event.value.value(QStringLiteral("expression")).toString())
                                    << QStringLiteral("deltaLen=%1").arg(event.delta.size());
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
    m_activeRunId.clear();
    m_finishPendingAfterStart = false;
    m_finishPendingAfterGate = false;
    if (m_runtime != nullptr) {
        m_runtime->setSuppressAutoIdle(false);
    }
    setSending(false);
    setStatusText(idleStatusText());
    if (!m_currentConversationId.isEmpty()) {
        loadConversations();
    }
}

void ChatController::failCurrentReply(const QString &message)
{
    const QString text = message.trimmed().isEmpty() ? QStringLiteral("请求失败") : message.trimmed();
    const bool hasBufferedText = !m_holdBuffer.isEmpty();

    ++m_asyncGeneration;
    m_gateTimeout.stop();
    m_pendingExpression.clear();
    m_pendingState.clear();
    m_finishPendingAfterStart = false;
    m_finishPendingAfterGate = false;
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
    m_activeRunId.clear();
    if (m_runtime != nullptr) {
        m_runtime->setSuppressAutoIdle(false);
    }
    setSending(false);
    setStatusText(QStringLiteral("错误"));
    requestPetExpression(QStringLiteral("error"), QStringLiteral("neutral"));
    if (!m_currentConversationId.isEmpty()) {
        loadConversations();
    }
}

void ChatController::abortPendingConversationCreate()
{
    ++m_pendingCreateRequestId;
    if (m_pendingConversationCreateReply) {
        QNetworkReply *reply = m_pendingConversationCreateReply;
        m_pendingConversationCreateReply.clear();
        reply->abort();
        reply->deleteLater();
    }
}

void ChatController::isolateConversationAsyncState()
{
    const bool shouldResetPetAnimation = m_sending
        || !m_currentReply.isNull()
        || !m_pendingConversationCreateReply.isNull()
        || m_phase != ChatPhase::IDLE;

    m_cancelled = true;
    ++m_asyncGeneration;
    ++m_chatRequestId;
    ++m_listRequestId;
    ++m_messageLoadRequestId;
    abortPendingConversationCreate();

    m_gateTimeout.stop();
    m_pendingExpression.clear();
    m_pendingState.clear();
    m_finishPendingAfterStart = false;
    m_finishPendingAfterGate = false;
    m_holdBuffer.clear();
    m_activeRunId.clear();
    transitionTo(ChatPhase::IDLE);
    if (m_runtime != nullptr) {
        m_runtime->setSuppressAutoIdle(false);
    }

    ++m_currentStreamId;
    if (m_pacer != nullptr) {
        m_pacer->discardBeforeStream(m_currentStreamId);
    }
    // 只有切走正在进行的聊天回复时才收回到 idle。
    // 启动恢复历史会话只是 UI 同步，不能打断 startup.briefcase 入场。
    if (shouldResetPetAnimation && m_runtime != nullptr) {
        m_runtime->returnToIdle();
    }

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
    setStatusText(idleStatusText());
}
