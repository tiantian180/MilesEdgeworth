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

    void append(const QString &text);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    int pendingCount() const { return m_queue.size(); }

signals:
    void chunkReady(const QString &chunk);

private:
    void tick();
    int effectiveInterval() const;

    QString m_queue;
    QTimer m_timer;
    int m_msPerChar = 80;

    static constexpr int kMaxBacklog = 30;
    static constexpr double kBacklogSpeedupFactor = 0.5;
};
