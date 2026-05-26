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
    int skinSchemaVersion = 1;
    QString minAppVersion;
    QUrl thumbnailUrl;
    // 文件系统皮肤根目录，形如 file:///.../skins/my-skin/。
    QUrl rootUrl;
    // 绝对文件系统路径，指向 skin.json 声明的 manifest 文件。
    QString manifestPath;
};
