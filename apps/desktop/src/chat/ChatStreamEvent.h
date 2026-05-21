#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

struct ChatStreamEvent
{
    QString type;
    QString name;
    QString runId;
    QString messageId;
    QString role;
    QString delta;
    QString error;
    QVariantMap value;
};

class ChatStreamEventParser
{
public:
    QList<ChatStreamEvent> ingest(const QByteArray &chunk);

private:
    QByteArray m_buffer;
};
