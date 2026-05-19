#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QString>

// SkinManifestLoader 只负责把 manifest.json 解析成 SkinManifest。
//
// 运行时不应该直接理解 JSON 结构细节；它只消费解析后的动作、配方、候选池
// 和交互区域。这样后续 manifest schema 调整时，影响面会集中在加载器。
class SkinManifestLoader
{
public:
    static SkinManifest loadFromResource(const QString &resourcePath);
    static SkinManifest fallbackManifest();
};
