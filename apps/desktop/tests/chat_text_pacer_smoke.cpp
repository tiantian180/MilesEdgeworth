#include "chat/ChatTextPacer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QSignalSpy>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <stdexcept>

namespace {

template <typename Predicate>
bool waitFor(QCoreApplication &app, int timeoutMs, Predicate predicate)
{
    QTimer deadline;
    deadline.setSingleShot(true);
    deadline.start(timeoutMs);
    while (!predicate() && deadline.isActive()) {
        app.processEvents(QEventLoop::AllEvents, 5);
    }
    return predicate();
}

QString assembleChunks(const QSignalSpy &chunks)
{
    QString assembled;
    for (const QList<QVariant> &args : chunks) {
        assembled.append(args.at(0).toString());
    }
    return assembled;
}

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

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(20);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        pacer.append(QStringLiteral("abc"));
        require(pacer.pendingCount() == 3, "pacer should queue all appended characters");

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        require(drained, "basic stream should drain");
        require(assembleChunks(chunks) == QStringLiteral("abc"), "basic stream should emit all chunks in order");
    }

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(15);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        pacer.append(QStringLiteral("异议"));

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        require(drained, "CJK stream should drain");
        require(assembleChunks(chunks) == QStringLiteral("异议"), "CJK stream should emit all chunks in order");
        require(chunks.size() == 2, "CJK stream should emit one chunk per character");
    }

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(120);
        require(pacer.msPerChar() == 120, "msPerChar getter should reflect setter value");
    }

    // --- Backlog catch-up: when queue exceeds maxBacklog, interval halves ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(40);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        // 60 chars > maxBacklog (30), so the effective interval should drop.
        QString longPayload;
        for (int i = 0; i < 60; ++i) {
            longPayload.append(QChar('x'));
        }

        const qint64 start = QDateTime::currentMSecsSinceEpoch();
        pacer.append(longPayload);

        const bool drained = waitFor(app, 3000, [&]() { return pacer.pendingCount() == 0; });
        require(drained, "backlog stream should drain");
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - start;

        require(elapsed < 2200, "backlog catch-up should drain faster than the no-speedup baseline");
        require(chunks.size() == 60, "backlog stream should emit every character");
    }

    // --- setMsPerChar updates an active timer interval ---
    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(200);
        pacer.append(QStringLiteral("yz"));
        pacer.setMsPerChar(15);

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        require(drained, "active timer should adopt updated msPerChar interval");
    }

    return 0;
}
