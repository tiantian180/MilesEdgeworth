#include "logging/MilesLogHandler.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>
#include <memory>

namespace {

QtMessageHandler previousHandler = nullptr;
QMutex logMutex;
std::unique_ptr<QFile> logFile;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO ");
    case QtWarningMsg:
        return QStringLiteral("WARN ");
    case QtCriticalMsg:
    case QtFatalMsg:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO ");
}

QString sourceName(const QMessageLogContext &context)
{
    if (context.file == nullptr || context.line <= 0) {
        return QString();
    }
    return QFileInfo(QString::fromUtf8(context.file)).fileName()
        + QStringLiteral(":")
        + QString::number(context.line);
}

void writeStderr(const QString &line)
{
    const QByteArray bytes = line.toLocal8Bit();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stderr);
    std::fwrite("\n", 1, 1, stderr);
    std::fflush(stderr);
}

void writeFileLine(const QString &line)
{
    if (logFile == nullptr || !logFile->isOpen()) {
        return;
    }

    QTextStream stream(logFile.get());
    stream << line << '\n';
    stream.flush();
}

void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&logMutex);
    if (MilesLogHandler::isMilesCategory(context.category)) {
        writeStderr(MilesLogHandler::formatMilesMessage(type, context, message, false));
        writeFileLine(MilesLogHandler::formatMilesMessage(type, context, message, true));
        return;
    }

    const QString defaultLine = qFormatLogMessage(type, context, message);
    if (previousHandler != nullptr) {
        previousHandler(type, context, message);
    } else {
        writeStderr(defaultLine);
    }

    writeFileLine(defaultLine);
}

} // namespace

namespace MilesLogHandler {

void install()
{
    const QByteArray path = qgetenv("MILES_LOG_FILE").trimmed();
    if (!path.isEmpty()) {
        logFile = std::make_unique<QFile>(QString::fromLocal8Bit(path));
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            logFile.reset();
        }
    }

    previousHandler = qInstallMessageHandler(handleMessage);
}

bool isMilesCategory(const char *category)
{
    if (category == nullptr) {
        return false;
    }
    return QByteArray(category).startsWith("miles.");
}

QString formatMilesMessage(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message,
                           bool includeDate)
{
    const QString timestamp = QDateTime::currentDateTime().toString(
        includeDate ? QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz") : QStringLiteral("HH:mm:ss.zzz"));
    const QString category = QString::fromUtf8(context.category == nullptr ? "miles.app" : context.category)
        .toUpper();
    QString line = timestamp
        + QStringLiteral(" ")
        + levelName(type)
        + QStringLiteral(" [")
        + category
        + QStringLiteral("] ")
        + message;

    const QString source = sourceName(context);
    if (!source.isEmpty()) {
        line += QStringLiteral(" [") + source + QStringLiteral("]");
    }
    return line;
}

} // namespace MilesLogHandler
