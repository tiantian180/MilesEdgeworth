#include "pet/manifest/PersonaStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>

namespace {
bool isValidSkinId(const QString &skinId)
{
    if (skinId.isEmpty()
        || skinId.contains(QLatin1Char('/'))
        || skinId.contains(QLatin1Char('\\'))
        || skinId.contains(QStringLiteral(".."))) {
        return false;
    }

    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return pattern.match(skinId).hasMatch();
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString qrcUrlToResourcePath(const QUrl &url)
{
    if (url.scheme() != QStringLiteral("qrc")) {
        return {};
    }
    return QStringLiteral(":") + url.path();
}

QString personaPathForRootUrl(const QUrl &rootUrl)
{
    if (rootUrl.isLocalFile()) {
        return QDir(rootUrl.toLocalFile()).filePath(QStringLiteral("persona.md"));
    }

    if (rootUrl.scheme() == QStringLiteral("qrc")) {
        QUrl personaUrl = rootUrl;
        QString path = personaUrl.path();
        if (!path.endsWith(QLatin1Char('/'))) {
            path.append(QLatin1Char('/'));
        }
        path.append(QStringLiteral("persona.md"));
        personaUrl.setPath(path);
        return qrcUrlToResourcePath(personaUrl);
    }

    return {};
}

bool writeTextFile(const QString &path, const QString &content, QString *errorMessage)
{
    const QFileInfo fileInfo(path);
    QDir parent(fileInfo.absolutePath());
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建 persona 目录：%1").arg(parent.absolutePath());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法写入 persona 文件：%1").arg(path);
        }
        return false;
    }

    if (file.write(content.toUtf8()) == -1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入 persona 文件失败：%1").arg(path);
        }
        return false;
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}
} // namespace

QString PersonaStore::dataDir()
{
    const QString envDir = qEnvironmentVariable("MILES_DATA_DIR").trimmed();
    if (!envDir.isEmpty()) {
        return envDir;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString PersonaStore::overridePathForSkin(const QString &skinId)
{
    if (!isValidSkinId(skinId)) {
        return {};
    }
    return QDir(dataDir()).filePath(QStringLiteral("persona-overrides/%1.md").arg(skinId));
}

QString PersonaStore::readForDescriptor(const SkinDescriptor &descriptor)
{
    if (!isValidSkinId(descriptor.id)) {
        return {};
    }

    const QString overridePath = overridePathForSkin(descriptor.id);
    if (QFileInfo::exists(overridePath) && QFileInfo(overridePath).isFile()) {
        return readTextFile(overridePath);
    }

    const QString skinPersonaPath = personaPathForRootUrl(descriptor.rootUrl);
    if (!skinPersonaPath.isEmpty() && QFileInfo::exists(skinPersonaPath) && QFileInfo(skinPersonaPath).isFile()) {
        return readTextFile(skinPersonaPath);
    }

    return {};
}

bool PersonaStore::writeForManifest(const SkinManifest &manifest, const QString &content, QString *errorMessage)
{
    if (!isValidSkinId(manifest.skinId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("皮肤 id 非法，无法保存 persona：%1").arg(manifest.skinId);
        }
        return false;
    }

    QString path;
    if (manifest.builtin) {
        path = overridePathForSkin(manifest.skinId);
    } else if (manifest.skinRootUrl.isLocalFile()) {
        path = QDir(manifest.skinRootUrl.toLocalFile()).filePath(QStringLiteral("persona.md"));
    }

    if (path.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前皮肤没有可写入的 persona 路径。");
        }
        return false;
    }

    return writeTextFile(path, content, errorMessage);
}
