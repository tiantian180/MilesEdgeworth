#pragma once

#include "pet/events/PetEvent.h"
#include "pet/manifest/SkinManifest.h"
#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <QList>

// CustomInteractionResult 把“是否继续默认逻辑”和“是否产生请求”分开。
//
// 高级交互可以只追加效果并继续默认行为，也可以完全接管事件。
// 这个边界后续会承接 JS/TS 或 C++ 皮肤脚本；当前先接 Miles 红茶命令
// miles.feedTea。
struct CustomInteractionResult
{
    QList<ActionRequest> requests;
    bool continueDefault = true;
};

class CustomInteractionRegistry
{
public:
    static CustomInteractionResult handleEvent(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const PetEvent &event
    );
};
