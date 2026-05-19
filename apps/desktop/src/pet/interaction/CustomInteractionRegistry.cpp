#include "pet/interaction/CustomInteractionRegistry.h"

namespace {
constexpr auto kFeedTeaCommandId = "miles.feedTea";
}

CustomInteractionResult CustomInteractionRegistry::handleEvent(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    Q_UNUSED(manifest);

    CustomInteractionResult result;

    if (event.type != PetEventType::MenuCommand || event.commandId != QString::fromUtf8(kFeedTeaCommandId)) {
        return result;
    }

    // 红茶是 Miles 皮肤定制命令。它不是 Runtime 通用能力，所以在
    // Custom Interaction 层转成 action pool 请求，并阻止默认菜单逻辑继续处理。
    result.continueDefault = false;
    if (snapshot.currentActionId != "sleep") {
        result.requests.append(ActionRequest::actionPool("menu.tea"));
    }

    return result;
}
