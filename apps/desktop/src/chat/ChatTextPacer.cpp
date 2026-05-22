#include "chat/ChatTextPacer.h"

#include <QChar>

ChatTextPacer::ChatTextPacer(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &ChatTextPacer::tick);
}

void ChatTextPacer::append(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    m_queue.append(text);
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

int ChatTextPacer::effectiveInterval() const
{
    if (m_queue.size() > kMaxBacklog) {
        const int sped = static_cast<int>(m_msPerChar * kBacklogSpeedupFactor);
        return sped < 1 ? 1 : sped;
    }
    return m_msPerChar;
}

void ChatTextPacer::tick()
{
    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }

    int popCount = 1;
    const QChar first = m_queue.at(0);
    if (first.isHighSurrogate() && m_queue.size() >= 2) {
        const QChar second = m_queue.at(1);
        if (second.isLowSurrogate()) {
            popCount = 2;
        }
    }

    const QString chunk = m_queue.left(popCount);
    m_queue.remove(0, popCount);
    emit chunkReady(chunk);

    if (m_queue.isEmpty()) {
        m_timer.stop();
        return;
    }

    const int next = effectiveInterval();
    if (m_timer.interval() != next) {
        m_timer.start(next);
    }
}
