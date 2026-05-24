#include "chat/ChatStreamEvent.h"

#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    ChatStreamEventParser parser;
    QList<ChatStreamEvent> events = parser.ingest(
        "data: {\"type\":\"RUN_STARTED\",\"runId\":\"mock-run\"}\n\n"
        "data: {\"type\":\"TEXT_MESSAGE_START\",\"messageId\":\"a1\",\"role\":\"assistant\"}\n\n"
    );

    require(events.size() == 2, "two complete events should parse");
    require(events[0].type == "RUN_STARTED", "first event type should be RUN_STARTED");
    require(events[0].runId == "mock-run", "runId should parse");
    require(events[1].type == "TEXT_MESSAGE_START", "second event type should be TEXT_MESSAGE_START");
    require(events[1].messageId == "a1", "messageId should parse");
    require(events[1].role == "assistant", "role should parse");

    events = parser.ingest("data: {\"type\":\"TEXT_MESSAGE_CONTENT\",\"messageId\":\"a1\",\"delta\":\"异");
    require(events.isEmpty(), "partial event should not parse");
    events = parser.ingest("议\"}\n\n");
    require(events.size() == 1, "partial event should complete after second chunk");
    require(events[0].type == "TEXT_MESSAGE_CONTENT", "content event should parse");
    require(events[0].delta == QStringLiteral("异议"), "delta should preserve unicode text");

    events = parser.ingest(
        "event: message\n"
        "data: {\"type\":\"CUSTOM\",\"name\":\"miles.pet.expression.requested\","
        "\"value\":{\"state\":\"speaking\",\"expression\":\"objection\"}}\n\n"
    );
    require(events.size() == 1, "custom event should parse");
    require(events[0].type == "CUSTOM", "custom type should parse");
    require(events[0].name == "miles.pet.expression.requested", "custom name should parse");
    require(events[0].value.value("state").toString() == "speaking", "custom state should parse");
    require(events[0].value.value("expression").toString() == "objection", "custom expression should parse");

    events = parser.ingest(
        "event: message\n"
        "data: {\"type\":\"CUSTOM\",\"name\":\"miles.pet.lifecycle\","
        "\"value\":{\"state\":\"thinking\"}}\n\n"
    );
    require(events.size() == 1, "lifecycle custom event should parse");
    require(events[0].name == "miles.pet.lifecycle", "lifecycle name should parse");
    require(events[0].value.value("state").toString() == "thinking", "lifecycle state should parse");

    events = parser.ingest("data: {not-json}\n\n");
    require(events.isEmpty(), "malformed JSON should be ignored");

    events = parser.ingest(":\n\n");
    require(events.isEmpty(), "comment-only frame should be ignored");

    events = parser.ingest("\n\n");
    require(events.isEmpty(), "blank frame should be ignored");

    events = parser.ingest("data: {\"runId\":\"missing-type\"}\n\n");
    require(events.isEmpty(), "event without type should be ignored");

    return 0;
}
