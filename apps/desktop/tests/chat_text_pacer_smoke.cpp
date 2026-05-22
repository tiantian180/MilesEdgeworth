#include "chat/ChatTextPacer.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QSignalSpy>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <cassert>

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(20);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        pacer.append(QStringLiteral("abc"));
        assert(pacer.pendingCount() == 3);

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);
        assert(assembleChunks(chunks) == QStringLiteral("abc"));
    }

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(15);
        QSignalSpy chunks(&pacer, &ChatTextPacer::chunkReady);

        pacer.append(QStringLiteral("异议"));

        const bool drained = waitFor(app, 1000, [&]() { return pacer.pendingCount() == 0; });
        assert(drained);
        assert(assembleChunks(chunks) == QStringLiteral("异议"));
        assert(chunks.size() == 2);
    }

    {
        ChatTextPacer pacer;
        pacer.setMsPerChar(120);
        assert(pacer.msPerChar() == 120);
    }

    return 0;
}
