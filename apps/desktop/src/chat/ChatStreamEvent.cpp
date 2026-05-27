#include "chat/ChatStreamEvent.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
int nextFrameDelimiter(const QByteArray &buffer, qsizetype *delimiterLength)
{
    const qsizetype lfIndex = buffer.indexOf("\n\n");
    const qsizetype crlfIndex = buffer.indexOf("\r\n\r\n");

    if (lfIndex < 0 && crlfIndex < 0) {
        return -1;
    }

    if (crlfIndex >= 0 && (lfIndex < 0 || crlfIndex < lfIndex)) {
        *delimiterLength = 4;
        return static_cast<int>(crlfIndex);
    }

    *delimiterLength = 2;
    return static_cast<int>(lfIndex);
}

QByteArray dataPayload(const QByteArray &frame)
{
    QByteArray payload;
    bool hasDataLine = false;
    const QList<QByteArray> lines = frame.split('\n');

    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        if (!line.startsWith("data:")) {
            continue;
        }

        QByteArray value = line.mid(5);
        if (value.startsWith(' ')) {
            value.remove(0, 1);
        }

        if (hasDataLine) {
            payload.append('\n');
        }
        payload.append(value);
        hasDataLine = true;
    }

    return payload;
}

ChatStreamEvent eventFromObject(const QJsonObject &object)
{
    ChatStreamEvent event;
    event.type = object.value("type").toString();
    event.name = object.value("name").toString();
    event.runId = object.value("runId").toString();
    event.messageId = object.value("messageId").toString();
    event.role = object.value("role").toString();
    event.delta = object.value("delta").toString();
    event.error = object.value("error").toString();
    event.toolCallId = object.value("toolCallId").toString();
    event.toolName = object.value("toolName").toString();
    event.toolArgs = object.value("toolArgs").toString();
    event.value = object.value("value").toObject().toVariantMap();
    return event;
}
} // namespace

QList<ChatStreamEvent> ChatStreamEventParser::ingest(const QByteArray &chunk)
{
    QList<ChatStreamEvent> events;
    m_buffer.append(chunk);

    qsizetype delimiterLength = 0;
    int delimiterIndex = nextFrameDelimiter(m_buffer, &delimiterLength);
    while (delimiterIndex >= 0) {
        const QByteArray frame = m_buffer.left(delimiterIndex);
        m_buffer.remove(0, delimiterIndex + delimiterLength);

        const QByteArray payload = dataPayload(frame);
        if (!payload.isEmpty()) {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
            if (error.error == QJsonParseError::NoError && document.isObject()) {
                const ChatStreamEvent event = eventFromObject(document.object());
                if (!event.type.isEmpty()) {
                    events.append(event);
                }
            }
        }

        delimiterIndex = nextFrameDelimiter(m_buffer, &delimiterLength);
    }

    return events;
}
