#include "logging/MilesLogHandler.h"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
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

    std::cout << "logging formatter smoke checks passed.\n";
    return 0;
}
