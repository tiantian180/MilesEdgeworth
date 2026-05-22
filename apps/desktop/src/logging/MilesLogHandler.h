#pragma once

#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>

namespace MilesLogHandler {

void install();
bool isMilesCategory(const char *category);
QString formatMilesMessage(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message,
                           bool includeDate);

} // namespace MilesLogHandler
