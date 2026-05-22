#include "chat/ChatController.h"
#include "chat/ChatStreamEvent.h"
#include "pet/PetRuntime.h"
#include "settings/SecretStore.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
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
    require(controller.messages().constFirst().toMap().value("text").toString() == QStringLiteral("异议"),
            "content delta should append to assistant message");

    ChatStreamEvent expressionEvent;
    expressionEvent.type = QStringLiteral("CUSTOM");
    expressionEvent.name = QStringLiteral("miles.pet.expression.requested");
    expressionEvent.value.insert(QStringLiteral("state"), QStringLiteral("speaking"));
    expressionEvent.value.insert(QStringLiteral("expression"), QStringLiteral("objection"));
    controller.applyStreamEvent(expressionEvent);
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

    // Hold buffer round-trip: feed a delta while the controller is in its default
    // "immediate flush" mode (Phase 2.1 plumbing -- full GATED logic lands in 2.3).
    PetRuntime hbRuntime;
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
                == QStringLiteral("片段一"),
            "hold buffer should flush content immediately in phase 2.1");

    hbContent.delta = QStringLiteral("片段二");
    hbController.applyStreamEvent(hbContent);
    require(hbController.messages().constFirst().toMap().value("text").toString()
                == QStringLiteral("片段一片段二"),
            "subsequent deltas should append through the hold buffer");

    return 0;
}
