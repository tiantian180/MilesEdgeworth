#pragma once

#include "pet/events/PetEvent.h"
#include "pet/manifest/SkinManifest.h"
#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <QList>

class CustomInteractionRegistry;
class HitZoneMatcher;
class BehaviorTriggerEngine;

// InteractionPipeline 把 PetEvent 转换为一个或多个 ActionRequest。
//
// 它只做纯决策：检查当前状态、命中区域、behavior trigger 和自定义交互。
// 真正播放动作仍由 PetRuntime::submitActionRequest 执行。
class InteractionPipeline
{
public:
    static QList<ActionRequest> handleEvent(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const PetEvent &event
    );
};
