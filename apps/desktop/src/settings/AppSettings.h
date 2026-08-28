#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

class AppSettings final : public QSettings
{
public:
    explicit AppSettings(QObject *parent = nullptr)
        : QSettings(filePath(), QSettings::IniFormat, parent)
    {
    }

    static QString filePath()
    {
        return QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("DesktopCat.ini"));
    }
};
