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
                "initial expression should apply after the synchronous clean finish boundary");

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

        const QString preBoundaryText = gController.messages().constLast()
                .toMap().value(QStringLiteral("text")).toString();
        require(preBoundaryText == QStringLiteral("片段1"),
                "GATED text should stay buffered before the animation boundary");
        require(gRuntime.currentState() == QStringLiteral("speaking"),
                "mid-run expression should not apply before the animation boundary");

        gRuntime.handleAnimationFinished();
        require(gRuntime.currentState() == QStringLiteral("thinking"),
                "boundary callback should request the queued thinking expression");
        require(gRuntime.currentActionId() == QStringLiteral("thinking"),
                "queued thinking expression should switch to the thinking action at the boundary");
        require(waitFor([&gController]() {
                    return gController.messages().constLast().toMap()
                        .value(QStringLiteral("text")).toString() == QStringLiteral("片段1片段2");
                }),
                "GATED text should drain through the pacer after the boundary callback");
    }

    // --- Phase 2.3.1 Task 7: RUN_FINISHED waits for animation boundary before idle ---
    {
        PetRuntime fRuntime;
        for (int i = 0; i < 5 && fRuntime.currentActionId() != QStringLiteral("idle_stand"); ++i) {
            fRuntime.handleAnimationFinished();
        }
        require(fRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "RUN_FINISHED boundary test should start from idle runtime state");

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
                "RUN_FINISHED boundary test should enter speaking before finish");
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
                "RUN_FINISHED boundary test should stream text before finish");

        ChatStreamEvent fFinished;
        fFinished.type = QStringLiteral("RUN_FINISHED");
        fFinished.runId = QStringLiteral("mock-run-finished");
        fController.applyStreamEvent(fFinished);

        require(!fController.sending(), "RUN_FINISHED should clear sending while waiting for boundary");
        require(fController.statusText() == QStringLiteral("未连接"),
                "RUN_FINISHED should restore non-sending status while waiting for boundary");
        require(!fController.messages().constLast().toMap().value(QStringLiteral("pending")).toBool(),
                "RUN_FINISHED should clear assistant pending before boundary");
        require(fRuntime.currentState() == QStringLiteral("speaking"),
                "RUN_FINISHED must not synchronously return to idle");
        require(fRuntime.currentActionId() == fSpeakingAction,
                "RUN_FINISHED must not synchronously replace the current animation");

        fRuntime.handleAnimationFinished();
        require(fRuntime.currentState() == QStringLiteral("idle"),
                "RUN_FINISHED should return to idle only after boundary callback");
        require(fRuntime.currentActionId() == QStringLiteral("idle_stand"),
                "RUN_FINISHED boundary callback should restore idle action");
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

    return 0;
}
