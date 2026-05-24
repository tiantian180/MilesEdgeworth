#include "chat/ChatTextPacer.h"

#include <QChar>

ChatTextPacer::ChatTextPacer(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &ChatTextPacer::tick);
}

void ChatTextPacer::append(const QString &text, quint64 streamId, int segmentId)
{
    if (text.isEmpty()) {
        return;
    }

    if (!m_queue.isEmpty()
            && m_queue.last().streamId == streamId
            && m_queue.last().segmentId == segmentId) {
        m_queue.last().text.append(text);
    } else {
        m_queue.append(QueuedChunk{text, streamId, segmentId});
    }
    if (!m_timer.isActive()) {
        m_timer.start(effectiveInterval());
    }
}

void ChatTextPacer::setMsPerChar(int value)
{
    if (value < 1) {
        value = 1;
    }
    if (value == m_msPerChar) {
        return;
    }

    m_msPerChar = value;
    if (m_timer.isActive()) {
        m_timer.start(effectiveInterval());
    }
}

void ChatTextPacer::discardBeforeStream(quint64 streamId)
{
    const bool hadQueuedText = !isEmpty();
    for (qsizetype i = m_queue.size() - 1; i >= 0; --i) {
        if (m_queue.at(i).streamId < streamId) {
            m_queue.removeAt(i);
        }
    }
    stopIfEmpty();
    if (hadQueuedText && isEmpty()) {
        emit pacerEmpty();
    }
}

int ChatTextPacer::pendingCountForSegment(int segmentId) const
{
    int total = 0;
    for (const QueuedChunk &chunk : m_queue) {
        if (chunk.segmentId == segmentId) {
            total += chunk.text.size();
        }
    }
    return total;
}

int ChatTextPacer::effectiveInterval() const
{
    if (queuedCharCount() > kMaxBacklog) {
        const int sped = static_cast<int>(m_msPerChar * kBacklogSpeedupFactor);
        return sped < 1 ? 1 : sped;
    }
    return m_msPerChar;
}

void ChatTextPacer::tick()
{
    if (isEmpty()) {
        m_timer.stop();
        return;
    }

    QueuedChunk &front = m_queue.first();
    int popCount = 1;
    const QChar first = front.text.at(0);
    if (first.isHighSurrogate() && front.text.size() >= 2) {
        const QChar second = front.text.at(1);
        if (second.isLowSurrogate()) {
            popCount = 2;
        }
    }

    const QString chunk = front.text.left(popCount);
    const quint64 streamId = front.streamId;
    const int segmentId = front.segmentId;
    front.text.remove(0, popCount);
    if (front.text.isEmpty()) {
        m_queue.removeFirst();
    }
    const bool segmentNowDrained = segmentId >= 0 && pendingCountForSegment(segmentId) == 0;
    emit chunkReady(chunk, streamId);
    if (segmentNowDrained) {
        emit segmentDrained(segmentId);
    }

    if (isEmpty()) {
        m_timer.stop();
        emit pacerEmpty();
        return;
    }

    const int next = effectiveInterval();
    if (m_timer.interval() != next) {
        m_timer.start(next);
    }
}

void ChatTextPacer::stopIfEmpty()
{
    if (isEmpty()) {
        m_timer.stop();
    }
}

bool ChatTextPacer::isEmpty() const
{
    return m_queue.isEmpty() || queuedCharCount() == 0;
}

qsizetype ChatTextPacer::queuedCharCount() const
{
    qsizetype total = 0;
    for (const QueuedChunk &chunk : m_queue) {
        total += chunk.text.size();
    }
    return total;
}
