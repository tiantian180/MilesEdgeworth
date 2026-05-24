#include "chat/ChatController.h"
#include "chat/ChatStreamEvent.h"
#include "pet/PetRuntime.h"
#include "settings/ProviderConfigFile.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTemporaryDir>

#include <functional>
#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool waitFor(const std::function<bool()> &predicate, int timeoutMs = 1500)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return predicate();
}

ProviderConfigFile::ModelConfig modelConfig(const QString &name,
                                            const QString &baseUrl,
                                            const QString &apiKey,
                                            const QString &model)
{
    ProviderConfigFile::ModelConfig cfg;
    cfg.name = name;
    cfg.baseUrl = baseUrl;
    cfg.apiKey = apiKey;
    cfg.model = model;
    return cfg;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir settingsDir;
    require(settingsDir.isValid(), "settings temp dir should be valid");
    SettingsService settings(settingsDir.filePath(QStringLiteral("settings.json")));
    auto cfg = modelConfig(QStringLiteral("test"),
                           QStringLiteral("https://api.example.test/v1"),
                           QStringLiteral("sk-test"),
                           QStringLiteral("miles-test-model"));
    cfg.temperature = 0.2;
    cfg.maxTokens = 128;
    require(settings.setModelConfig(cfg.name, cfg), "settings should accept model config");
    settings.setActiveModelConfig(cfg.name);
    settings.setMsPerChar(40);

    PetRuntime runtime;
    ChatController controller(&runtime, &settings);

    const QMetaObject *metaObject = controller.metaObject();
    require(metaObject->indexOfProperty("conversations") >= 0,
            "ChatController should expose conversations as a Qt property");
    require(metaObject->indexOfProperty("currentConversationId") >= 0,
            "ChatController should expose currentConversationId as a Qt property");
    require(metaObject->indexOfProperty("conversationSkinMismatch") >= 0,
            "ChatController should expose conversationSkinMismatch as a Qt property");
    require(metaObject->indexOfProperty("conversationSkinHint") >= 0,
            "ChatController should expose conversationSkinHint as a Qt property");
    require(metaObject->indexOfProperty("providerConfigured") >= 0,
            "ChatController should expose providerConfigured as a Qt property");
    require(!controller.conversationSkinMismatch(),
            "conversation skin mismatch should be false by default");
    require(controller.conversationSkinHint().isEmpty(),
            "conversation skin hint should be empty by default");

    // 启动后恢复历史会话只是 UI 状态同步，不应该打断桌宠入场 recipe。
    {
        PetRuntime startupRuntime;
        require(startupRuntime.currentActionId() == QStringLiteral("briefcase_in"),
                "startup conversation restore test should begin during briefcase_in");
        ChatController startupController(&startupRuntime, &settings);

        startupController.switchConversation(QStringLiteral("restored-conversation"));
        require(startupRuntime.currentActionId() == QStringLiteral("briefcase_in"),
                "switchConversation without an active chat stream must not interrupt startup animation");
        require(startupRuntime.currentRecipeId() == QStringLiteral("startup.briefcase"),
                "switchConversation without an active chat stream must keep the startup recipe active");
    }

    {
        SettingsService providerSettings(settingsDir.filePath(QStringLiteral("provider-settings.json")));
        auto incomplete = modelConfig(QStringLiteral("test"),
                                      QStringLiteral("https://api.example.test/v1"),
                                      QString(),
                                      QStringLiteral("miles-test-model"));
        require(providerSettings.setModelConfig(incomplete.name, incomplete),
                "settings should accept incomplete config");
        providerSettings.setActiveModelConfig(incomplete.name);
        ChatController providerController(&runtime, &providerSettings);
        require(!providerController.providerConfigured(),
                "providerConfigured should stay false until base URL, API key, and model are all present");

        incomplete.apiKey = QStringLiteral("sk-test");
        require(providerSettings.setModelConfig(incomplete.name, incomplete),
                "settings should update provider config");
        providerController.handleSettingsSaved();
        require(providerController.providerConfigured(),
                "providerConfigured should become true after complete provider settings are saved");
    }

    ChatStreamEvent started;
    started.type = QStringLiteral("RUN_STARTED");
    started.runId = QStringLiteral("mock-run");
    controller.applyStreamEvent(started);
    require(controller.sending(), "RUN_STARTED should mark controller as sending");
    require(controller.statusText() == QStringLiteral("正在回复"),
            "RUN_STARTED should set replying status");

    ChatStreamEvent memoryEvent;
    memoryEvent.type = QStringLiteral("CUSTOM");
    memoryEvent.name = QStringLiteral("miles.chat.memory.summarizing");
    controller.applyStreamEvent(memoryEvent);
    require(controller.statusText() == QStringLiteral("整理记忆中..."),
            "memory summarizing event should update status text");

    controller.applyStreamEvent(started);
    require(controller.statusText() == QStringLiteral("正在回复"),
            "RUN_STARTED should override memory summarizing status");

    ChatStreamEvent thinkingEvent;
    thinkingEvent.type = QStringLiteral("CUSTOM");
    thinkingEvent.name = QStringLiteral("miles.pet.lifecycle");
    thinkingEvent.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
    controller.applyStreamEvent(thinkingEvent);
    for (int i = 0; i < 5 && runtime.currentState() != QStringLiteral("thinking"); ++i) {
        runtime.handleAnimationFinished();
    }
    require(runtime.currentState() == QStringLiteral("thinking"), "custom thinking event should update runtime state");
    require(runtime.currentActionId() == QStringLiteral("thinking"), "thinking neutral should play thinking action");

    ChatStreamEvent messageStart;
    messageStart.type = QStringLiteral("TEXT_MESSAGE_START");
    messageStart.messageId = QStringLiteral("assistant-1");
    messageStart.role = QStringLiteral("assistant");
    controller.applyStreamEvent(messageStart);
    require(controller.messages().size() == 1, "assistant message should be appended");

    ChatStreamEvent messageContent;
    messageContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    messageContent.messageId = QStringLiteral("assistant-1");
    messageContent.delta = QStringLiteral("异议");
    controller.applyStreamEvent(messageContent);
    require(controller.messages().constFirst().toMap().value("text").toString().isEmpty(),
            "content delta without an active segment should still wait for the pacer");
    require(waitFor([&controller]() {
                return controller.messages().constFirst().toMap().value("text").toString()
                    == QStringLiteral("异议");
            }),
            "content delta should append to assistant message through the pacer");

    ChatStreamEvent expressionEvent;
    expressionEvent.type = QStringLiteral("CUSTOM");
    expressionEvent.name = QStringLiteral("miles.pet.expression.requested");
    expressionEvent.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
    expressionEvent.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
    controller.applyStreamEvent(expressionEvent);
    for (int i = 0; i < 5 && runtime.currentState() != QStringLiteral("speaking"); ++i) {
        runtime.handleAnimationFinished();
    }
    require(waitFor([&runtime]() {
                return runtime.currentState() == QStringLiteral("speaking");
            }),
            "custom expression event should update runtime state after the gate opens");
    require(runtime.currentState() == QStringLiteral("speaking"), "custom expression event should update runtime state");
    require(runtime.currentActionId() == QStringLiteral("objecting")
                || runtime.currentActionId() == QStringLiteral("crossed"),
            "speaking objection should play a valid speaking action");
    const QString speakingAction = runtime.currentActionId();
    if (speakingAction == QStringLiteral("objecting")) {
        require(!runtime.currentAutoReturnToIdle(),
                "entry-only speaking objection action should stay held until cleanFinish handles the transition");
    }

    ChatStreamEvent idleAfterCurrentEvent;
    idleAfterCurrentEvent.type = QStringLiteral("CUSTOM");
    idleAfterCurrentEvent.name = QStringLiteral("miles.pet.expression.requested");
    idleAfterCurrentEvent.value.insert(QStringLiteral("state"), QStringLiteral("idle"));
    idleAfterCurrentEvent.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
    idleAfterCurrentEvent.value.insert(QStringLiteral("interruptHint"), QStringLiteral("afterCurrent"));
    controller.applyStreamEvent(idleAfterCurrentEvent);
    require(runtime.currentState() == QStringLiteral("speaking"), "afterCurrent idle expression should not change state immediately");
    require(runtime.currentActionId() == speakingAction, "afterCurrent idle expression should not interrupt the current speaking action");

    ChatStreamEvent finished;
    finished.type = QStringLiteral("RUN_FINISHED");
    finished.runId = QStringLiteral("mock-run");
    controller.applyStreamEvent(finished);
    require(!controller.sending(), "RUN_FINISHED should clear sending");
    require(runtime.currentState() == QStringLiteral("speaking"), "RUN_FINISHED should not interrupt the current speaking state");
    require(runtime.currentActionId() == speakingAction, "RUN_FINISHED should not restart or replace the current speaking action");

    runtime.handleAnimationFinished();
    require(runtime.currentState() == QStringLiteral("idle"), "pending afterCurrent idle expression should run after speaking animation finishes");
    require(runtime.currentActionId() == QStringLiteral("idle_stand"), "pending afterCurrent idle expression should return to idle action");

    ChatStreamEvent errorEvent;
    errorEvent.type = QStringLiteral("RUN_ERROR");
    errorEvent.error = QStringLiteral("mock failure");
    controller.applyStreamEvent(errorEvent);
    require(runtime.currentState() == QStringLiteral("error"), "RUN_ERROR should move pet to error state");

    // 切换会话后，旧 SSE stream 的残留事件不应该写入新会话消息模型。
    {
        PetRuntime staleRuntime;
        ChatController staleController(&staleRuntime, &settings);

        ChatStreamEvent staleStarted;
        staleStarted.type = QStringLiteral("RUN_STARTED");
        staleController.applyStreamEvent(staleStarted);
        staleRuntime.handleAnimationFinished();

        ChatStreamEvent staleStart;
        staleStart.type = QStringLiteral("TEXT_MESSAGE_START");
        staleStart.role = QStringLiteral("assistant");
        staleController.applyStreamEvent(staleStart);
        require(staleController.messages().size() == 1,
                "stale stream test should start with one assistant message");

        staleController.switchConversation(QStringLiteral("new-conversation"));
        require(staleController.messages().isEmpty(),
                "switchConversation should clear the visible message model");

        ChatStreamEvent staleContent;
        staleContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        staleContent.delta = QStringLiteral("旧回复");
        staleController.applyStreamEvent(staleContent);
        waitFor([]() { return false; }, 120);
        require(staleController.messages().isEmpty(),
                "stream content after switching conversation must not append to the new message model");
    }

    // 切换会话必须隔离旧 runtime 回调，并让桌宠离开旧回复动画状态。
    {
        QTcpServer holdServer;
        QList<QTcpSocket *> heldSockets;
        const bool holdServerListening = holdServer.listen(QHostAddress::LocalHost, 39710);
        if (holdServerListening) {
            QObject::connect(&holdServer, &QTcpServer::newConnection, &holdServer, [&holdServer, &heldSockets]() {
                while (holdServer.hasPendingConnections()) {
                    QTcpSocket *socket = holdServer.nextPendingConnection();
                    socket->setParent(&holdServer);
                    heldSockets.append(socket);
                }
            });
        }

        PetRuntime staleCallbackRuntime;
        for (int i = 0; i < 5 && staleCallbackRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            staleCallbackRuntime.handleAnimationFinished();
        }
        staleCallbackRuntime.requestExpression(QStringLiteral("speaking"), QStringLiteral("objection"));
        require(staleCallbackRuntime.currentState() == QStringLiteral("speaking"),
                "stale callback setup should enter speaking before switching conversations");
        ChatController staleCallbackController(&staleCallbackRuntime, &settings);

        ChatStreamEvent oldStarted;
        oldStarted.type = QStringLiteral("RUN_STARTED");
        staleCallbackController.applyStreamEvent(oldStarted);
        waitFor([]() { return false; }, 1000);

        ChatStreamEvent secondStarted;
        secondStarted.type = QStringLiteral("RUN_STARTED");
        staleCallbackController.applyStreamEvent(secondStarted);
        staleCallbackController.switchConversation(QStringLiteral("new-conversation"));
        require(staleCallbackRuntime.currentState() == QStringLiteral("idle"),
                "switchConversation should return runtime to idle instead of leaving old speaking state active");

        if (holdServerListening) {
            staleCallbackController.sendMessage(QStringLiteral("next"));

            ChatStreamEvent newStarted;
            newStarted.type = QStringLiteral("RUN_STARTED");
            staleCallbackController.applyStreamEvent(newStarted);

            ChatStreamEvent newStart;
            newStart.type = QStringLiteral("TEXT_MESSAGE_START");
            newStart.role = QStringLiteral("assistant");
            staleCallbackController.applyStreamEvent(newStart);

            ChatStreamEvent newText;
            newText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
            newText.delta = QStringLiteral("新回复");
            staleCallbackController.applyStreamEvent(newText);

            waitFor([]() { return false; }, 700);
            require(staleCallbackController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString().isEmpty(),
                    "stale clean-finish callback must not release a later stream's held text");

            bool currentStreamReleased = false;
            for (int i = 0; i < 8 && !currentStreamReleased; ++i) {
                staleCallbackRuntime.handleAnimationFinished();
                currentStreamReleased = waitFor([&staleCallbackController]() {
                    return staleCallbackController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("新回复");
                }, 150);
            }
            require(currentStreamReleased,
                    "current stream clean-finish callback should still release held text");
        }
    }

    // 取消后到达的残留事件不应该新建 assistant 消息
    controller.switchConversation(QStringLiteral("smoke-conversation"));
    controller.sendMessage(QStringLiteral("again"));
    const int messagesBeforeCancel = controller.messages().size();
    controller.cancelCurrentReply();
    require(!controller.sending(), "cancel should clear sending");

    ChatStreamEvent strayContent;
    strayContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    strayContent.messageId = QStringLiteral("assistant-2");
    strayContent.delta = QStringLiteral("残留 token");
    controller.applyStreamEvent(strayContent);
    require(controller.messages().size() == messagesBeforeCancel,
            "stream content after cancel must not append a new assistant message");

    // Hold buffer round-trip: feed deltas while the controller is waiting for
    // cleanFinishReady, then let the pacer drain after the animation clean finish.
    PetRuntime hbRuntime;
    hbRuntime.setState(QStringLiteral("speaking"));
    ChatController hbController(&hbRuntime, &settings);

    ChatStreamEvent hbStarted;
    hbStarted.type = QStringLiteral("RUN_STARTED");
    hbController.applyStreamEvent(hbStarted);

    ChatStreamEvent hbStart;
    hbStart.type = QStringLiteral("TEXT_MESSAGE_START");
    hbStart.role = QStringLiteral("assistant");
    hbController.applyStreamEvent(hbStart);

    ChatStreamEvent hbContent;
    hbContent.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
    hbContent.delta = QStringLiteral("片段一");
    hbController.applyStreamEvent(hbContent);

    require(hbController.messages().constFirst().toMap().value("text").toString()
                .isEmpty(),
            "hold buffer should not flush content before cleanFinishReady");
    hbRuntime.handleAnimationFinished();
    require(waitFor([&hbController]() {
                return hbController.messages().constFirst().toMap().value("text").toString()
                    == QStringLiteral("片段一");
            }),
            "hold buffer should drain through the pacer after cleanFinishReady");

    hbContent.delta = QStringLiteral("片段二");
    hbController.applyStreamEvent(hbContent);
    require(waitFor([&hbController]() {
                return hbController.messages().constFirst().toMap().value("text").toString()
                    == QStringLiteral("片段一片段二");
            }),
            "subsequent deltas should append through the pacer");

    // --- Phase 2.3.1: BUFFERING_FOR_START holds text until cleanFinishReady ---
    {
        PetRuntime smRuntime;
        smRuntime.setState(QStringLiteral("speaking")); // simulate non-idle baseline
        ChatController smController(&smRuntime, &settings);

        ChatStreamEvent smStarted;
        smStarted.type = QStringLiteral("RUN_STARTED");
        smController.applyStreamEvent(smStarted);
        // BUFFERING_FOR_START: text must be queued, not emitted to UI yet.

        ChatStreamEvent smExpr;
        smExpr.type = QStringLiteral("CUSTOM");
        smExpr.name = QStringLiteral("miles.pet.expression.requested");
        smExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        smExpr.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        smController.applyStreamEvent(smExpr);

        ChatStreamEvent smStart;
        smStart.type = QStringLiteral("TEXT_MESSAGE_START");
        smStart.role = QStringLiteral("assistant");
        smController.applyStreamEvent(smStart);

        ChatStreamEvent smText;
        smText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        smText.delta = QStringLiteral("异议!");
        smController.applyStreamEvent(smText);

        const auto messagesWhileBuffering = smController.messages();
        const QString textWhileBuffering = messagesWhileBuffering.constLast()
                .toMap().value(QStringLiteral("text")).toString();
        require(textWhileBuffering.isEmpty(),
                "BUFFERING_FOR_START must NOT push streamed text to the UI yet");

        // Simulate PetRuntime reaching a clean finish on the previous animation.
        smRuntime.handleAnimationFinished();
        require(waitFor([&smController]() {
                    const auto messages = smController.messages();
                    return !messages.isEmpty()
                        && messages.constLast().toMap().value(QStringLiteral("text")).toString().startsWith(QStringLiteral("异"));
                }),
                "cleanFinishReady should let buffered text start draining through the pacer");
        // Full pacer drain timing is covered by ChatTextPacerSmoke.
    }

    // --- Fast response: RUN_FINISHED must not discard the start expression while buffering ---
    {
        PetRuntime fastRuntime;
        fastRuntime.setState(QStringLiteral("thinking"));
        ChatController fastController(&fastRuntime, &settings);

        ChatStreamEvent fastStarted;
        fastStarted.type = QStringLiteral("RUN_STARTED");
        fastController.applyStreamEvent(fastStarted);

        ChatStreamEvent fastExpr;
        fastExpr.type = QStringLiteral("CUSTOM");
        fastExpr.name = QStringLiteral("miles.pet.expression.requested");
        fastExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        fastExpr.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        fastController.applyStreamEvent(fastExpr);

        ChatStreamEvent fastStart;
        fastStart.type = QStringLiteral("TEXT_MESSAGE_START");
        fastStart.role = QStringLiteral("assistant");
        fastController.applyStreamEvent(fastStart);

        ChatStreamEvent fastText;
        fastText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        fastText.delta = QStringLiteral("快速异议");
        fastController.applyStreamEvent(fastText);

        ChatStreamEvent fastEnd;
        fastEnd.type = QStringLiteral("TEXT_MESSAGE_END");
        fastController.applyStreamEvent(fastEnd);

        ChatStreamEvent fastFinished;
        fastFinished.type = QStringLiteral("RUN_FINISHED");
        fastController.applyStreamEvent(fastFinished);

        require(fastRuntime.currentActionId() != QStringLiteral("objecting"),
                "fast response should still wait for clean finish before applying start expression");

        for (int i = 0;
             i < 5
             && fastRuntime.currentActionId() != QStringLiteral("objecting")
             && fastRuntime.currentActionId() != QStringLiteral("crossed");
             ++i) {
            fastRuntime.handleAnimationFinished();
        }
        require(fastRuntime.currentActionId() == QStringLiteral("objecting")
                    || fastRuntime.currentActionId() == QStringLiteral("crossed"),
                "RUN_FINISHED during BUFFERING_FOR_START must preserve and apply the pending objection expression");
        require(waitFor([&fastController]() {
                    return fastController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("快速异议");
                }),
                "fast buffered text should drain after the preserved start expression applies");
    }

    // --- Phase 2.3.1: mid-stream expression switch (STREAMING -> GATED -> STREAMING) ---
    {
        PetRuntime gRuntime;
        for (int i = 0; i < 5 && gRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            gRuntime.handleAnimationFinished();
        }
        require(gRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "mid-stream gate test should start from idle runtime state");

        ChatController gController(&gRuntime, &settings);

        ChatStreamEvent gStarted;
        gStarted.type = QStringLiteral("RUN_STARTED");
        gController.applyStreamEvent(gStarted);

        ChatStreamEvent gExpr1;
        gExpr1.type = QStringLiteral("CUSTOM");
        gExpr1.name = QStringLiteral("miles.pet.expression.requested");
        gExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        gExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        gController.applyStreamEvent(gExpr1);
        require(gRuntime.currentState() == QStringLiteral("speaking"),
                "initial expression should apply after synchronous cleanFinishReady");

        ChatStreamEvent gStart;
        gStart.type = QStringLiteral("TEXT_MESSAGE_START");
        gStart.role = QStringLiteral("assistant");
        gController.applyStreamEvent(gStart);

        ChatStreamEvent gText1;
        gText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gText1.delta = QStringLiteral("片段1");
        gController.applyStreamEvent(gText1);
        require(waitFor([&gController]() {
                    return gController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("片段1");
                }),
                "STREAMING text should drain through the pacer before the mid-run gate");

        ChatStreamEvent gExpr2;
        gExpr2.type = QStringLiteral("CUSTOM");
        gExpr2.name = QStringLiteral("miles.pet.expression.requested");
        gExpr2.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        gExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        gController.applyStreamEvent(gExpr2);

        ChatStreamEvent gText2;
        gText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gText2.delta = QStringLiteral("片段2");
        gController.applyStreamEvent(gText2);

        const QString preCleanFinishText = gController.messages().constLast()
                .toMap().value(QStringLiteral("text")).toString();
        require(preCleanFinishText == QStringLiteral("片段1"),
                "GATED text should stay buffered before cleanFinishReady");
        require(gRuntime.currentState() == QStringLiteral("speaking"),
                "mid-run expression should not apply before cleanFinishReady");

        gRuntime.handleAnimationFinished();
        require(gRuntime.currentState() == QStringLiteral("thinking"),
                "cleanFinish callback should request the queued thinking expression");
        require(gRuntime.currentActionId() == QStringLiteral("thinking"),
                "queued thinking expression should switch to the thinking action at cleanFinishReady");
        require(waitFor([&gController]() {
                    return gController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("片段1片段2");
                }),
                "GATED text should drain through the pacer after the cleanFinish callback");
    }

    // 旧 cleanFinish 回调被 gate timeout 释放后，不能再释放同一回复里的下一次 GATED 文本。
    {
        PetRuntime timeoutRuntime;
        for (int i = 0; i < 5 && timeoutRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            timeoutRuntime.handleAnimationFinished();
        }

        ChatController timeoutController(&timeoutRuntime, &settings);

        ChatStreamEvent timeoutStarted;
        timeoutStarted.type = QStringLiteral("RUN_STARTED");
        timeoutController.applyStreamEvent(timeoutStarted);

        ChatStreamEvent timeoutExpr1;
        timeoutExpr1.type = QStringLiteral("CUSTOM");
        timeoutExpr1.name = QStringLiteral("miles.pet.expression.requested");
        timeoutExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        timeoutExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        timeoutController.applyStreamEvent(timeoutExpr1);

        ChatStreamEvent timeoutStart;
        timeoutStart.type = QStringLiteral("TEXT_MESSAGE_START");
        timeoutStart.role = QStringLiteral("assistant");
        timeoutController.applyStreamEvent(timeoutStart);

        ChatStreamEvent timeoutText1;
        timeoutText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        timeoutText1.delta = QStringLiteral("一");
        timeoutController.applyStreamEvent(timeoutText1);
        require(waitFor([&timeoutController]() {
                    return timeoutController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("一");
                }),
                "timeout gate setup should enter streaming and drain first text");

        ChatStreamEvent timeoutExpr2;
        timeoutExpr2.type = QStringLiteral("CUSTOM");
        timeoutExpr2.name = QStringLiteral("miles.pet.expression.requested");
        timeoutExpr2.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        timeoutExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        timeoutController.applyStreamEvent(timeoutExpr2);

        ChatStreamEvent timeoutText2;
        timeoutText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        timeoutText2.delta = QStringLiteral("二");
        timeoutController.applyStreamEvent(timeoutText2);
        require(waitFor([&timeoutController]() {
                    return timeoutController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("一二");
                }, 2600),
                "gate timeout should release the first gated text");

        ChatStreamEvent timeoutExpr3;
        timeoutExpr3.type = QStringLiteral("CUSTOM");
        timeoutExpr3.name = QStringLiteral("miles.pet.expression.requested");
        timeoutExpr3.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        timeoutExpr3.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        timeoutController.applyStreamEvent(timeoutExpr3);

        ChatStreamEvent timeoutText3;
        timeoutText3.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        timeoutText3.delta = QStringLiteral("三");
        timeoutController.applyStreamEvent(timeoutText3);

        waitFor([]() { return false; }, 650);
        require(timeoutController.messages().constLast().toMap()
                    .value(QStringLiteral("text")).toString() == QStringLiteral("一二"),
                "stale cleanFinish safety timeout must not release the next gated text early");
        require(waitFor([&timeoutController]() {
                    return timeoutController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("一二三");
                }, 2500),
                "the current gate timeout should still release its own buffered text");
    }

    // RUN_FINISHED during a GATED expression switch must flush the final held text
    // and wait for the new expression to clean-finish before idling.
    {
        PetRuntime gfRuntime;
        for (int i = 0; i < 5 && gfRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            gfRuntime.handleAnimationFinished();
        }

        ChatController gfController(&gfRuntime, &settings);

        ChatStreamEvent gfStarted;
        gfStarted.type = QStringLiteral("RUN_STARTED");
        gfController.applyStreamEvent(gfStarted);

        ChatStreamEvent gfExpr1;
        gfExpr1.type = QStringLiteral("CUSTOM");
        gfExpr1.name = QStringLiteral("miles.pet.expression.requested");
        gfExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        gfExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        gfController.applyStreamEvent(gfExpr1);
        require(gfRuntime.currentState() == QStringLiteral("speaking"),
                "GATED finish test should start speaking after initial cleanFinish");

        ChatStreamEvent gfStart;
        gfStart.type = QStringLiteral("TEXT_MESSAGE_START");
        gfStart.role = QStringLiteral("assistant");
        gfController.applyStreamEvent(gfStart);

        ChatStreamEvent gfText1;
        gfText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gfText1.delta = QStringLiteral("前");
        gfController.applyStreamEvent(gfText1);
        require(waitFor([&gfController]() {
                    return gfController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前");
                }),
                "GATED finish test should stream first text before the gate");

        ChatStreamEvent gfExpr2;
        gfExpr2.type = QStringLiteral("CUSTOM");
        gfExpr2.name = QStringLiteral("miles.pet.expression.requested");
        gfExpr2.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        gfExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        gfController.applyStreamEvent(gfExpr2);

        ChatStreamEvent gfText2;
        gfText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        gfText2.delta = QStringLiteral("后");
        gfController.applyStreamEvent(gfText2);

        ChatStreamEvent gfFinished;
        gfFinished.type = QStringLiteral("RUN_FINISHED");
        gfController.applyStreamEvent(gfFinished);

        require(!gfController.sending(),
                "RUN_FINISHED during GATED should clear sending while waiting for cleanFinish");
        require(gfController.messages().constLast().toMap()
                    .value(QStringLiteral("text")).toString() == QStringLiteral("前"),
                "RUN_FINISHED during GATED should keep final text held before cleanFinish");

        for (int i = 0; i < 5 && gfRuntime.currentState() != QStringLiteral("thinking"); ++i) {
            gfRuntime.handleAnimationFinished();
        }
        require(waitFor([&gfController]() {
                    return gfController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前后");
                }),
                "RUN_FINISHED during GATED should drain final held text at cleanFinish");
        require(gfRuntime.currentState() == QStringLiteral("thinking"),
                "RUN_FINISHED during GATED should activate the final gated expression first");

        for (int i = 0; i < 5 && gfRuntime.currentState() != QStringLiteral("idle"); ++i) {
            gfRuntime.handleAnimationFinished();
        }
        require(gfRuntime.currentState() == QStringLiteral("idle"),
                "RUN_FINISHED during GATED should idle only after final expression cleanFinish");
    }

    // --- Phase 2.3.1 Task 7: RUN_FINISHED waits for cleanFinishReady before idle ---
    {
        PetRuntime fRuntime;
        for (int i = 0; i < 5 && fRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            fRuntime.handleAnimationFinished();
        }
        require(fRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "RUN_FINISHED cleanFinish test should start from idle runtime state");

        ChatController fController(&fRuntime, &settings);

        ChatStreamEvent fStarted;
        fStarted.type = QStringLiteral("RUN_STARTED");
        fController.applyStreamEvent(fStarted);

        ChatStreamEvent fExpr;
        fExpr.type = QStringLiteral("CUSTOM");
        fExpr.name = QStringLiteral("miles.pet.expression.requested");
        fExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        fExpr.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        fController.applyStreamEvent(fExpr);
        require(fRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED cleanFinish test should enter speaking before finish");
        const QString fSpeakingAction = fRuntime.currentActionId();

        ChatStreamEvent fStart;
        fStart.type = QStringLiteral("TEXT_MESSAGE_START");
        fStart.role = QStringLiteral("assistant");
        fController.applyStreamEvent(fStart);

        ChatStreamEvent fText;
        fText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        fText.delta = QStringLiteral("结尾");
        fController.applyStreamEvent(fText);
        require(waitFor([&fController]() {
                    return fController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("结尾");
                }),
                "RUN_FINISHED cleanFinish test should stream text before finish");

        ChatStreamEvent fFinished;
        fFinished.type = QStringLiteral("RUN_FINISHED");
        fFinished.runId = QStringLiteral("mock-run-finished");
        fController.applyStreamEvent(fFinished);

        require(!fController.sending(), "RUN_FINISHED should clear sending while waiting for cleanFinishReady");
        require(fController.statusText() == QStringLiteral("未连接"),
                "RUN_FINISHED should restore non-sending status while waiting for cleanFinishReady");
        require(!fController.messages().constLast().toMap().value(QStringLiteral("pending")).toBool(),
                "RUN_FINISHED should clear assistant pending before cleanFinishReady");
        require(fRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED must not synchronously return to idle");
        require(fRuntime.currentActionId() == fSpeakingAction,
                "RUN_FINISHED must not synchronously replace the current animation");

        fRuntime.handleAnimationFinished();
        require(fRuntime.currentState() == QStringLiteral("idle"),
                "RUN_FINISHED should return to idle only after cleanFinish callback");
        require(fRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "RUN_FINISHED cleanFinish callback should restore idle action");
    }

    // --- Phase 2.3.1 Task 8: cancel during GATED drains buffered text and idles ---
    {
        PetRuntime cRuntime;
        for (int i = 0; i < 5 && cRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            cRuntime.handleAnimationFinished();
        }
        require(cRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "GATED cancel test should start from idle runtime state");

        ChatController cController(&cRuntime, &settings);

        ChatStreamEvent cStarted;
        cStarted.type = QStringLiteral("RUN_STARTED");
        cController.applyStreamEvent(cStarted);

        ChatStreamEvent cExpr1;
        cExpr1.type = QStringLiteral("CUSTOM");
        cExpr1.name = QStringLiteral("miles.pet.expression.requested");
        cExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        cExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        cController.applyStreamEvent(cExpr1);
        require(cRuntime.currentState() == QStringLiteral("speaking"),
                "GATED cancel test should enter speaking before cancel");

        ChatStreamEvent cStart;
        cStart.type = QStringLiteral("TEXT_MESSAGE_START");
        cStart.role = QStringLiteral("assistant");
        cController.applyStreamEvent(cStart);

        ChatStreamEvent cText1;
        cText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        cText1.delta = QStringLiteral("前");
        cController.applyStreamEvent(cText1);
        require(waitFor([&cController]() {
                    return cController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前");
                }),
                "GATED cancel test should stream the first segment before the gate");

        ChatStreamEvent cExpr2;
        cExpr2.type = QStringLiteral("CUSTOM");
        cExpr2.name = QStringLiteral("miles.pet.expression.requested");
        cExpr2.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        cExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        cController.applyStreamEvent(cExpr2);

        ChatStreamEvent cText2;
        cText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        cText2.delta = QStringLiteral("后");
        cController.applyStreamEvent(cText2);
        require(cController.messages().constLast().toMap()
                    .value(QStringLiteral("text")).toString() == QStringLiteral("前"),
                "GATED cancel test should hold text before cancel");

        const int messagesBeforeGatedCancel = cController.messages().size();
        cController.cancelCurrentReply();
        require(!cController.sending(), "cancel during GATED should clear sending");
        require(cRuntime.currentState() == QStringLiteral("idle"),
                "cancel during GATED should return runtime to idle");
        require(cRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "cancel during GATED should restore idle action");
        require(waitFor([&cController]() {
                    return cController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前后");
                }),
                "cancel during GATED should drain held text through the pacer");

        ChatStreamEvent cStray;
        cStray.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        cStray.delta = QStringLiteral("残留");
        cController.applyStreamEvent(cStray);
        require(cController.messages().size() == messagesBeforeGatedCancel,
                "stream content after GATED cancel must not append a new assistant message");
    }

    // --- Phase 2.3.1 Task 8: RUN_ERROR during GATED drains buffered text without ghost messages ---
    {
        PetRuntime eRuntime;
        for (int i = 0; i < 5 && eRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            eRuntime.handleAnimationFinished();
        }
        require(eRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "GATED error test should start from idle runtime state");

        ChatController eController(&eRuntime, &settings);

        ChatStreamEvent eStarted;
        eStarted.type = QStringLiteral("RUN_STARTED");
        eController.applyStreamEvent(eStarted);

        ChatStreamEvent eExpr1;
        eExpr1.type = QStringLiteral("CUSTOM");
        eExpr1.name = QStringLiteral("miles.pet.expression.requested");
        eExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        eExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        eController.applyStreamEvent(eExpr1);

        ChatStreamEvent eStart;
        eStart.type = QStringLiteral("TEXT_MESSAGE_START");
        eStart.role = QStringLiteral("assistant");
        eController.applyStreamEvent(eStart);

        ChatStreamEvent eText1;
        eText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        eText1.delta = QStringLiteral("前");
        eController.applyStreamEvent(eText1);
        require(waitFor([&eController]() {
                    return eController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前");
                }),
                "GATED error test should stream the first segment before the gate");

        ChatStreamEvent eExpr2;
        eExpr2.type = QStringLiteral("CUSTOM");
        eExpr2.name = QStringLiteral("miles.pet.expression.requested");
        eExpr2.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        eExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
        eController.applyStreamEvent(eExpr2);

        ChatStreamEvent eText2;
        eText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        eText2.delta = QStringLiteral("后");
        eController.applyStreamEvent(eText2);
        const int messagesBeforeError = eController.messages().size();

        ChatStreamEvent eError;
        eError.type = QStringLiteral("RUN_ERROR");
        eError.error = QStringLiteral("mock failure");
        eController.applyStreamEvent(eError);

        require(!eController.sending(), "RUN_ERROR during GATED should clear sending");
        require(eRuntime.currentState() == QStringLiteral("error"),
                "RUN_ERROR during GATED should move pet to error state");
        require(waitFor([&eController]() {
                    return eController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前后");
                }),
                "RUN_ERROR during GATED should drain held text through the pacer");
        require(eController.messages().size() == messagesBeforeError,
                "RUN_ERROR during GATED must not create a ghost assistant message");
        require(eController.messages().constLast().toMap().value(QStringLiteral("error")).toBool(),
                "RUN_ERROR during GATED should mark the assistant message as error");
    }

    // --- Phase 2.4: expression after unsegmented text must wait for pacerEmpty ---
    {
        SettingsService preTextSettings(settingsDir.filePath(QStringLiteral("pretext-settings.json")));
        auto preTextCfg = modelConfig(QStringLiteral("pretext"),
                                      QStringLiteral("https://api.example.test/v1"),
                                      QStringLiteral("sk-test"),
                                      QStringLiteral("miles-test-model"));
        require(preTextSettings.setModelConfig(preTextCfg.name, preTextCfg),
                "pretext settings should accept config");
        preTextSettings.setActiveModelConfig(preTextCfg.name);
        preTextSettings.setMsPerChar(80);

        PetRuntime pRuntime;
        for (int i = 0; i < 5 && pRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            pRuntime.handleAnimationFinished();
        }
        ChatController pController(&pRuntime, &preTextSettings);

        ChatStreamEvent pStarted;
        pStarted.type = QStringLiteral("RUN_STARTED");
        pController.applyStreamEvent(pStarted);

        ChatStreamEvent pStart;
        pStart.type = QStringLiteral("TEXT_MESSAGE_START");
        pStart.role = QStringLiteral("assistant");
        pController.applyStreamEvent(pStart);

        ChatStreamEvent pText;
        pText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        pText.delta = QStringLiteral("前前前");
        pController.applyStreamEvent(pText);

        ChatStreamEvent pExpr;
        pExpr.type = QStringLiteral("CUSTOM");
        pExpr.name = QStringLiteral("miles.pet.expression.requested");
        pExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        pExpr.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        pController.applyStreamEvent(pExpr);

        pRuntime.handleAnimationFinished();
        require(pRuntime.currentActionId() != QStringLiteral("bow"),
                "expression after unsegmented text must wait for pacerEmpty");
        require(waitFor([&pController]() {
                    return pController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("前前前");
                }, 1000),
                "unsegmented text should drain before the following expression activates");
        require(pRuntime.currentActionId() == QStringLiteral("bow"),
                "expression after unsegmented text should activate after pacerEmpty and cleanFinish");
    }

    // --- Phase 2.4: multiple expression segments must drain one by one ---
    {
        PetRuntime qRuntime;
        for (int i = 0; i < 5 && qRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            qRuntime.handleAnimationFinished();
        }
        ChatController qController(&qRuntime, &settings);

        ChatStreamEvent qStarted;
        qStarted.type = QStringLiteral("RUN_STARTED");
        qController.applyStreamEvent(qStarted);

        ChatStreamEvent qExpr1;
        qExpr1.type = QStringLiteral("CUSTOM");
        qExpr1.name = QStringLiteral("miles.pet.expression.requested");
        qExpr1.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        qExpr1.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
        qController.applyStreamEvent(qExpr1);

        ChatStreamEvent qStart;
        qStart.type = QStringLiteral("TEXT_MESSAGE_START");
        qStart.role = QStringLiteral("assistant");
        qController.applyStreamEvent(qStart);

        ChatStreamEvent qText1;
        qText1.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        qText1.delta = QStringLiteral("第一段");
        qController.applyStreamEvent(qText1);

        ChatStreamEvent qExpr2;
        qExpr2.type = QStringLiteral("CUSTOM");
        qExpr2.name = QStringLiteral("miles.pet.expression.requested");
        qExpr2.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        qExpr2.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        qController.applyStreamEvent(qExpr2);

        ChatStreamEvent qText2;
        qText2.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        qText2.delta = QStringLiteral("第二段");
        qController.applyStreamEvent(qText2);

        require(qRuntime.currentActionId() != QStringLiteral("bow"),
                "second expression must not activate before the first segment cleanFinish is ready");
        qRuntime.handleAnimationFinished();
        require(qRuntime.currentActionId() != QStringLiteral("bow"),
                "second expression must not activate before first segment text drains");
        require(qRuntime.currentState() == QStringLiteral("speaking"),
                "first queued expression should stay active until the dual gate opens");
        require(waitFor([&qController]() {
                    return qController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("第一段");
                }),
                "first segment text should drain before second segment starts");

        require(qRuntime.currentActionId() == QStringLiteral("bow"),
                "polite segment should activate bow after cleanFinish and first segment drain");
        require(waitFor([&qController]() {
                    return qController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("第一段第二段");
                }),
                "second segment text should drain after second expression activates");
    }

    // --- Phase 2.4: lifecycle thinking must clean-finish when RUN_FINISHED arrives during start buffering ---
    {
        PetRuntime ltRuntime;
        require(ltRuntime.currentActionId() == QStringLiteral("briefcase_in"),
                "lifecycle finish test should start before startup cleanFinish");
        ChatController ltController(&ltRuntime, &settings);

        ChatStreamEvent ltStarted;
        ltStarted.type = QStringLiteral("RUN_STARTED");
        ltController.applyStreamEvent(ltStarted);

        ChatStreamEvent ltThinking;
        ltThinking.type = QStringLiteral("CUSTOM");
        ltThinking.name = QStringLiteral("miles.pet.lifecycle");
        ltThinking.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
        ltController.applyStreamEvent(ltThinking);

        ChatStreamEvent ltFinished;
        ltFinished.type = QStringLiteral("RUN_FINISHED");
        ltController.applyStreamEvent(ltFinished);

        for (int i = 0; i < 5 && ltRuntime.currentState() != QStringLiteral("thinking"); ++i) {
            ltRuntime.handleAnimationFinished();
        }
        require(ltRuntime.currentState() == QStringLiteral("thinking"),
                "pending lifecycle thinking should apply after start cleanFinish");
        require(ltRuntime.currentState() != QStringLiteral("idle"),
                "pending lifecycle thinking must not idle immediately when stream already finished");

        for (int i = 0; i < 5 && ltRuntime.currentState() != QStringLiteral("idle"); ++i) {
            ltRuntime.handleAnimationFinished();
        }
        require(ltRuntime.currentState() == QStringLiteral("idle"),
                "lifecycle thinking should return to idle after its own cleanFinish");
    }

    // --- Phase 2.4: RUN_FINISHED waits for pacerEmpty before idle ---
    {
        SettingsService slowSettings(settingsDir.filePath(QStringLiteral("slow-settings.json")));
        auto slowCfg = modelConfig(QStringLiteral("slow"),
                                   QStringLiteral("https://api.example.test/v1"),
                                   QStringLiteral("sk-test"),
                                   QStringLiteral("miles-test-model"));
        require(slowSettings.setModelConfig(slowCfg.name, slowCfg), "slow settings should accept config");
        slowSettings.setActiveModelConfig(slowCfg.name);
        slowSettings.setMsPerChar(80);

        PetRuntime wRuntime;
        for (int i = 0; i < 5 && wRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            wRuntime.handleAnimationFinished();
        }
        ChatController wController(&wRuntime, &slowSettings);

        ChatStreamEvent wStarted;
        wStarted.type = QStringLiteral("RUN_STARTED");
        wController.applyStreamEvent(wStarted);

        ChatStreamEvent wExpr;
        wExpr.type = QStringLiteral("CUSTOM");
        wExpr.name = QStringLiteral("miles.pet.expression.requested");
        wExpr.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
        wExpr.value.insert(QStringLiteral("expression"), QStringLiteral("polite"));
        wController.applyStreamEvent(wExpr);

        ChatStreamEvent wStart;
        wStart.type = QStringLiteral("TEXT_MESSAGE_START");
        wStart.role = QStringLiteral("assistant");
        wController.applyStreamEvent(wStart);

        ChatStreamEvent wText;
        wText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        wText.delta = QStringLiteral("长长长长长");
        wController.applyStreamEvent(wText);
        wRuntime.handleAnimationFinished();

        ChatStreamEvent wFinished;
        wFinished.type = QStringLiteral("RUN_FINISHED");
        wController.applyStreamEvent(wFinished);
        wRuntime.handleAnimationFinished();

        require(wRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED cleanFinish should not idle before pacerEmpty");
        require(waitFor([&wController]() {
                    return wController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("长长长长长");
                }, 1500),
                "final text should drain after RUN_FINISHED");
        require(waitFor([&wRuntime]() {
                    return wRuntime.currentState() == QStringLiteral("idle");
                }, 1500),
                "runtime should idle after cleanFinish and pacerEmpty are both ready");
    }

    // --- Phase 2.3.1: old pacer chunks must not leak into a later reply ---
    {
        PetRuntime lRuntime;
        for (int i = 0; i < 5 && lRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            lRuntime.handleAnimationFinished();
        }

        ChatController lController(&lRuntime, &settings);

        ChatStreamEvent firstStarted;
        firstStarted.type = QStringLiteral("RUN_STARTED");
        lController.applyStreamEvent(firstStarted);

        ChatStreamEvent firstStart;
        firstStart.type = QStringLiteral("TEXT_MESSAGE_START");
        firstStart.role = QStringLiteral("assistant");
        lController.applyStreamEvent(firstStart);

        ChatStreamEvent firstText;
        firstText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        firstText.delta = QStringLiteral("旧旧旧旧旧旧旧旧旧旧");
        lController.applyStreamEvent(firstText);

        ChatStreamEvent firstFinished;
        firstFinished.type = QStringLiteral("RUN_FINISHED");
        lController.applyStreamEvent(firstFinished);
        lRuntime.handleAnimationFinished();
        const int messagesAfterFirst = lController.messages().size();

        ChatStreamEvent secondStarted;
        secondStarted.type = QStringLiteral("RUN_STARTED");
        lController.applyStreamEvent(secondStarted);

        ChatStreamEvent secondStart;
        secondStart.type = QStringLiteral("TEXT_MESSAGE_START");
        secondStart.role = QStringLiteral("assistant");
        lController.applyStreamEvent(secondStart);

        const int messagesAfterSecondStart = lController.messages().size();
        require(messagesAfterSecondStart == messagesAfterFirst + 1,
                "second reply setup should append one assistant message");

        waitFor([]() { return false; }, 200);

        const auto messages = lController.messages();
        require(messages.size() == messagesAfterSecondStart,
                "stale pacer chunks must not create ghost messages after a new reply starts");
        require(messages.constLast().toMap().value(QStringLiteral("text")).toString().isEmpty(),
                "stale pacer chunks must not leak into the next assistant message");
    }

    // --- Phase 2.3.1: sendMessage must invalidate stale pacer chunks before RUN_STARTED ---
    {
        PetRuntime sRuntime;
        for (int i = 0; i < 5 && sRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            sRuntime.handleAnimationFinished();
        }

        ChatController sController(&sRuntime, &settings);

        ChatStreamEvent firstStarted;
        firstStarted.type = QStringLiteral("RUN_STARTED");
        sController.applyStreamEvent(firstStarted);

        ChatStreamEvent firstStart;
        firstStart.type = QStringLiteral("TEXT_MESSAGE_START");
        firstStart.role = QStringLiteral("assistant");
        sController.applyStreamEvent(firstStart);

        ChatStreamEvent firstText;
        firstText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        firstText.delta = QStringLiteral("旧旧旧旧旧旧旧旧旧旧");
        sController.applyStreamEvent(firstText);

        ChatStreamEvent firstFinished;
        firstFinished.type = QStringLiteral("RUN_FINISHED");
        sController.applyStreamEvent(firstFinished);
        sRuntime.handleAnimationFinished();

        sController.switchConversation(QStringLiteral("smoke-conversation"));
        sController.sendMessage(QStringLiteral("next"));
        waitFor([]() { return false; }, 200);

        const auto messages = sController.messages();
        require(!messages.constLast().toMap().value(QStringLiteral("text")).toString().contains(QStringLiteral("旧")),
                "sendMessage should invalidate stale pacer chunks before sidecar RUN_STARTED arrives");

        ChatStreamEvent nextStarted;
        nextStarted.type = QStringLiteral("RUN_STARTED");
        sController.applyStreamEvent(nextStarted);
        sRuntime.handleAnimationFinished();
        ChatStreamEvent nextText;
        nextText.type = QStringLiteral("TEXT_MESSAGE_CONTENT");
        nextText.delta = QStringLiteral("新");
        sController.applyStreamEvent(nextText);
        require(waitFor([&sController]() {
                    return sController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString().contains(QStringLiteral("新"));
                }, 500),
                "new reply text should not wait for stale pacer chunks to drain");
    }

    return 0;
}
