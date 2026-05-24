#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// ChatTextPacer drives streamed text into the UI at a human reading rate.
// See docs/v2/设计方案/AI 聊天动画编排设计.md §7.
class ChatTextPacer : public QObject
{
    Q_OBJECT

public:
    explicit ChatTextPacer(QObject *parent = nullptr);

    void append(const QString &text, quint64 streamId = 0, int segmentId = -1);
    void discardBeforeStream(quint64 streamId);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    int pendingCount() const { return static_cast<int>(queuedCharCount()); }
    int pendingCountForSegment(int segmentId) const;

signals:
    void chunkReady(const QString &chunk, quint64 streamId);
    void segmentDrained(int segmentId);
    void pacerEmpty();

private:
    struct QueuedChunk {
        QString text;
        quint64 streamId = 0;
        int segmentId = -1;
    };

    void tick();
    int effectiveInterval() const;
    void stopIfEmpty();
    bool isEmpty() const;
    qsizetype queuedCharCount() const;

    QList<QueuedChunk> m_queue;
    QTimer m_timer;
    int m_msPerChar = 80;

    static constexpr int kMaxBacklog = 30;
    static constexpr double kBacklogSpeedupFactor = 0.5;
};
