#pragma once

#include "pet/manifest/SkinManifest.h"

// 把 Miles 皮肤需要的 Custom Interaction 注册到 CustomInteractionRegistry。
//
// 当前只注册 ProsecutorBadgeInteraction（双击概率丢检察官徽章）。
// main.cpp 在加载 PetRuntime 后调用一次；只有当 manifest.customInteractions
// 真的包含对应 id 时才会实际注册，避免其他皮肤被强行接入 Miles 玩法。
void registerMilesEdgeworthInteractions(const SkinManifest &manifest);
