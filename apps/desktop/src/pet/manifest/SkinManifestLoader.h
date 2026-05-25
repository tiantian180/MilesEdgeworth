#pragma once

#include "pet/manifest/SkinDescriptor.h"
#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

// SkinManifestLoader 只负责把 manifest.json 解析成 SkinManifest。
//
// 运行时不应该直接理解 JSON 结构细节；它只消费解析后的动作、配方、候选池
// 和交互区域。这样后续 manifest schema 调整时，影响面会集中在加载器。
class SkinManifestLoader
{
public:
    static SkinManifest loadFromResource(const QString &resourcePath);
    static SkinManifest loadFromDescriptor(const SkinDescriptor &descriptor);
    static SkinManifest loadFromDirectory(const QString &filesystemPath);
    static SkinManifest fallbackManifest();

    static QList<SkinDescriptor> discoverAll();
    static QList<SkinDescriptor> discoverInDirectories(const QStringList &directories, bool includeBuiltins = true);
    static QString userSkinDirectoryPath();
    static QString portableSkinDirectoryPath();

    // Schema v4: file: paths are skin-root relative; other URL schemes are already absolute.
    static QUrl resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl);
};
