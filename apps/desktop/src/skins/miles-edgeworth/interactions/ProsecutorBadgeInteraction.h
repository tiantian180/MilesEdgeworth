#pragma once

#include "pet/interaction/CustomInteractionRegistry.h"

class ProsecutorBadgeInteraction final : public CustomInteraction
{
public:
    QString id() const override;
    QSet<PetEventType> supportedEvents() const override;
    CustomInteractionOutcome handleEvent(
        const PetEvent &event,
        const RuntimeSnapshot &snapshot,
        CustomInteractionHostApi &host
    ) override;
};
