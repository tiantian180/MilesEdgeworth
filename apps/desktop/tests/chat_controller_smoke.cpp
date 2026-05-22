#include "chat/ChatController.h"
#include "chat/ChatStreamEvent.h"
#include "pet/PetRuntime.h"
#include "settings/SecretStore.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

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
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    NullSecretStore secretStore;
    SettingsService settings(&secretStore);
    settings.setBaseUrl(QStringLiteral("https://api.example.test/v1"));
    settings.setModel(QStringLiteral("miles-test-model"));
    settings.setTemperature(0.2);
    settings.setMaxTokens(128);
    settings.setMsPerChar(40);

    PetRuntime runtime;
    ChatController controller(&runtime, &settings);

    ChatStreamEvent started;
    started.type = QStringLiteral("RUN_STARTED");
    started.runId = QStringLiteral("mock-run");
    controller.applyStreamEvent(started);
    require(controller.sending(), "RUN_STARTED should mark controller as sending");

    ChatStreamEvent thinkingEvent;
    thinkingEvent.type = QStringLiteral("CUSTOM");
    thinkingEvent.name = QStringLiteral("miles.pet.expression.requested");
    thinkingEvent.value.insert(QStringLiteral("state"), QStringLiteral("thinking"));
    thinkingEvent.value.insert(QStringLiteral("expression"), QStringLiteral("neutral"));
    controller.applyStreamEvent(thinkingEvent);
    runtime.handleAnimationFinished();
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
    require(waitFor([&runtime]() {
                return runtime.currentState() == QStringLiteral("speaking");
            }),
            "custom expression event should update runtime state after the gate opens");
    require(runtime.currentState() == QStringLiteral("speaking"), "custom expression event should update runtime state");
    require(runtime.currentActionId() == QStringLiteral("objecting")
                || runtime.currentActionId() == QStringLiteral("crossed"),
            "speaking objection should play a valid speaking action");
    const QString speakingAction = runtime.currentActionId();
    require(runtime.currentAutoReturnToIdle(), "speaking objection action should return to idle after animation completion");

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

    // 取消后到达的残留事件不应该新建 assistant 消息
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
    // cleanFinishReady, then let the pacer drain after the animation boundary.
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

    return 0;
}
