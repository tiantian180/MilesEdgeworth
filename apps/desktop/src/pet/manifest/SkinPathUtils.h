#pragma once

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace SkinPathUtils {

inline bool pathIsInsideRoot(const QString &path, const QString &rootPath)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanRoot = QDir::cleanPath(rootPath);
    return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

inline QString canonicalRootPath(const QUrl &rootUrl)
{
    if (!rootUrl.isLocalFile()) {
        return {};
    }

    const QString rootPath = QDir(rootUrl.toLocalFile()).absolutePath();
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        return {};
    }
    return rootInfo.canonicalFilePath();
}

inline bool isResolvedLocalFileUrl(const QUrl &url)
{
    return url.isValid() && url.isLocalFile() && !url.toLocalFile().isEmpty();
}

inline bool existingLocalFileIsInsideRoot(const QUrl &url, const QUrl &rootUrl)
{
    if (!isResolvedLocalFileUrl(url)) {
        return false;
    }

    const QString rootPath = canonicalRootPath(rootUrl);
    if (rootPath.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(url.toLocalFile());
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString filePath = fileInfo.canonicalFilePath();
    if (filePath.isEmpty()) {
        return false;
    }

    return pathIsInsideRoot(filePath, rootPath);
}

inline QUrl idleStandAnimationUrlForSkinDirectory(const QString &skinDirectoryPath)
{
    return QUrl::fromLocalFile(
        QDir(skinDirectoryPath).filePath(QStringLiteral("assets/body/idle/stand-right.gif"))
    );
}

} // namespace SkinPathUtils
