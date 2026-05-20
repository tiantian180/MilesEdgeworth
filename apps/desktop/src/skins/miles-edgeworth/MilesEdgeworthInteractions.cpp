#include "skins/miles-edgeworth/MilesEdgeworthInteractions.h"

#include "pet/interaction/CustomInteractionRegistry.h"
#include "skins/miles-edgeworth/interactions/ProsecutorBadgeInteraction.h"

#include <memory>

void registerMilesEdgeworthInteractions(const SkinManifest &manifest)
{
    // Miles 示例皮肤的高级交互在这里集中注册。
    // 通用 Registry 不认识任何 Miles 动作名，避免定制玩法回流到框架层。
    if (manifest.customInteractions.contains(QStringLiteral("prosecutor_badge"))) {
        CustomInteractionRegistry::registerInteraction(std::make_unique<ProsecutorBadgeInteraction>());
    }
}
