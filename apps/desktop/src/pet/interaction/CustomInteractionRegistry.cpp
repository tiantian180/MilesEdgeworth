#include "pet/interaction/CustomInteractionRegistry.h"

CustomInteractionResult CustomInteractionRegistry::handleEvent(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    Q_UNUSED(manifest);
    Q_UNUSED(snapshot);
    Q_UNUSED(event);

    return {};
}
