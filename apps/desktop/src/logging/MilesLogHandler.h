#pragma once

#include <QtGlobal>

class QMessageLogContext;
class QString;

namespace MilesLogHandler {

void install();
bool isMilesCategory(const char *category);
QString formatMilesMessage(QtMsgType type,
                           const QMessageLogContext &context,
                           const QString &message,
                           bool includeDate);

} // namespace MilesLogHandler
