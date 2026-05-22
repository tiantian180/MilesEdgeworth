#include "logging/MilesLogHandler.h"

#include <QCoreApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

Q_LOGGING_CATEGORY(smokeLog, "miles.chat", QtInfoMsg)

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

QString readFile(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly | QIODevice::Text),
            "smoke log file should be readable");
    return QString::fromUtf8(file.readAll());
}

void appendSentinel(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text),
            "smoke log file should be appendable");
    QTextStream stream(&file);
    stream << "SIDEcar_APPEND_SENTINEL\n";
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    require(MilesLogHandler::isMilesCategory("miles.chat"),
            "miles.chat should be treated as project category");
    require(!MilesLogHandler::isMilesCategory("qt.qml.binding"),
            "qt.qml.binding should not be treated as project category");

    QMessageLogContext context("ChatController.cpp", 302, "sendMessage", "miles.chat");
    const QString consoleLine = MilesLogHandler::formatMilesMessage(
        QtInfoMsg,
        context,
        QStringLiteral("send chat request messageLen=12"),
        false);
    require(consoleLine.contains(QStringLiteral("INFO  [MILES.CHAT]")),
            "console line should contain padded INFO level and upper category");
    require(consoleLine.contains(QStringLiteral("send chat request messageLen=12")),
            "console line should contain message");
    require(consoleLine.endsWith(QStringLiteral("[ChatController.cpp:302]")),
            "console line should end with source");
    require(!consoleLine.left(10).contains('-'),
            "console line should omit date");

    const QString fileLine = MilesLogHandler::formatMilesMessage(
        QtDebugMsg,
        context,
        QStringLiteral("stream event type=CUSTOM"),
        true);
    require(fileLine.contains(QStringLiteral("DEBUG [MILES.CHAT]")),
            "file line should contain DEBUG level and category");
    require(fileLine.left(10).contains('-'),
            "file line should include date");

    QTemporaryDir tempDir;
    require(tempDir.isValid(), "temporary directory should be valid");
    const QString logPath = tempDir.filePath(QStringLiteral("miles-smoke.log"));
    qputenv("MILES_LOG_FILE", logPath.toLocal8Bit());

    MilesLogHandler::install();
    MilesLogHandler::install();

    qCInfo(smokeLog) << "first smoke line";
    appendSentinel(logPath);
    qCInfo(smokeLog) << "second smoke line";

    const QString logContent = readFile(logPath);
    require(logContent.contains(QStringLiteral("first smoke line")),
            "file log should contain first Qt line");
    require(logContent.contains(QStringLiteral("SIDEcar_APPEND_SENTINEL")),
            "file log should preserve external append sentinel");
    require(logContent.contains(QStringLiteral("second smoke line")),
            "file log should contain later Qt line");

    std::atomic_bool nonMilesReturned = false;
    std::thread nonMilesThread([&nonMilesReturned]() {
        qInfo("non miles smoke line");
        nonMilesReturned.store(true);
    });
    nonMilesThread.detach();

    for (int i = 0; i < 50 && !nonMilesReturned.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    require(nonMilesReturned.load(),
            "non-miles log after double install should not recurse or deadlock");

    std::cout << "logging formatter smoke checks passed.\n";
    return 0;
}
