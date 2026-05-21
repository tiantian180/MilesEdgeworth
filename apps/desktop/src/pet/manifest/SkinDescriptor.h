#pragma once

#include <QUrl>
#include <QString>

// SkinDescriptor 是扫描阶段使用的轻量元信息。
// 它只来自 skin.json，不解析完整 manifest，避免应用启动时读取所有动作配置。
struct SkinDescriptor
{
    QString id;
    QString name;
    QString version;
    QString author;
    QString license;
    int manifestVersion = 1;
    QString minAppVersion;
    QUrl thumbnailUrl;

    // rootUrl 是资源根目录。内置皮肤形如 qrc:/skins/miles-edgeworth/，
    // 文件系统皮肤形如 file:///.../skins/my-skin/。
    QUrl rootUrl;

    // manifestPath 是 Qt 可直接读取的 manifest.json 位置。
    // 内置皮肤使用 :/skins/miles-edgeworth/manifest.json，
    // 文件系统皮肤使用 /absolute/path/to/skin/manifest.json。
    QString manifestPath;

    bool builtin = false;
};
