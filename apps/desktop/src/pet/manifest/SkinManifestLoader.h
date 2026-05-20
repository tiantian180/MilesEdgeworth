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
    // 从 Qt Resource 路径加载 manifest（当前 Miles 内置皮肤路径形如 ":/skins/miles-edgeworth/manifest.json"）。
    // 文件系统皮肤包（详见《皮肤包分发与加载机制设计》）后续在此基础上扩展 loadFromDirectory。
    static SkinManifest loadFromResource(const QString &resourcePath);

    // 当资源加载失败时返回的兜底 manifest，保证桌宠至少能以 idle_stand 跑起来。
    static SkinManifest fallbackManifest();
};
