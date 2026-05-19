#include "pet/interaction/InteractionPipeline.h"

#include "pet/behavior/BehaviorTriggerEngine.h"
#include "pet/commands/SkinCommandResolver.h"
#include "pet/interaction/CustomInteractionRegistry.h"
#include "pet/interaction/HitZoneMatcher.h"

namespace {
constexpr auto kSleepToggleCommandId = "runtime.sleep.toggle";
constexpr auto kReturnToIdleCommandId = "runtime.returnToIdle";
constexpr auto kFacingToggleCommandId = "runtime.facing.toggle";

void appendIfPlayable(QList<ActionRequest> &requests, const ActionRequest &request)
{
    if (request.kind != ActionRequestKind::None) {
        requests.append(request);
    }
}
} // namespace

QList<ActionRequest> InteractionPipeline::handleEvent(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    QList<ActionRequest> requests;

    const CustomInteractionResult customResult = CustomInteractionRegistry::handleEvent(manifest, snapshot, event);
    requests.append(customResult.requests);
    if (!customResult.continueDefault) {
        return requests;
    }

    switch (event.type) {
    case PetEventType::PointerSingleClick: {
        if (!snapshot.pointerInteractionEnabled || snapshot.currentActionId == "sleep") {
            return requests;
        }

        const QString idleAction = manifest.stateToAction.value("idle");
        if (!idleAction.isEmpty() && snapshot.currentActionId != idleAction) {
            requests.append(ActionRequest::returnToIdle());
            return requests;
        }

        const HitZoneMatchContext hitZoneContext {
            snapshot.currentFacing,
            manifest.defaultFacing,
        };
        const QString poolId = HitZoneMatcher::clickPoolForPoint(
            manifest,
            hitZoneContext,
            event.x,
            event.y,
            event.width,
            event.height
        );
        if (!poolId.isEmpty()) {
            requests.append(ActionRequest::actionPool(poolId));
        }
        return requests;
    }

    case PetEventType::PointerDoubleClick:
        if (!snapshot.pointerInteractionEnabled) {
            return requests;
        }
        if (snapshot.currentActionId == "sleep" || snapshot.currentPhaseId == "loop") {
            requests.append(ActionRequest::returnToIdle());
            return requests;
        }
        requests.append(ActionRequest::actionPool("doubleClick.random"));
        return requests;

    case PetEventType::MenuCommand:
        if (event.commandId == QString::fromUtf8(kSleepToggleCommandId)) {
            if (snapshot.sleepTransitioning) {
                return requests;
            }
            if (snapshot.sleeping) {
                requests.append(ActionRequest::returnToIdle());
            } else {
                requests.append(ActionRequest::recipe("sleep.enterLoopExit"));
            }
        } else if (event.commandId == QString::fromUtf8(kReturnToIdleCommandId)) {
            requests.append(ActionRequest::returnToIdle());
        } else if (event.commandId == QString::fromUtf8(kFacingToggleCommandId)) {
            requests.append(ActionRequest::toggleFacing());
        } else {
            appendIfPlayable(requests, SkinCommandResolver::resolveCommand(manifest, snapshot, event.commandId));
        }
        return requests;

    case PetEventType::IdleLoopFinished: {
        if (!manifest.behaviorTriggers.contains("idle.loopFinished")) {
            return requests;
        }

        const BehaviorTriggerDefinition trigger = manifest.behaviorTriggers.value("idle.loopFinished");
        const BehaviorTriggerContext context {
            snapshot.currentState,
            snapshot.currentActionId,
            !snapshot.currentRecipeId.isEmpty(),
        };

        if (!BehaviorTriggerEngine::matches(trigger, context)) {
            return requests;
        }

        const BehaviorTriggerEntry entry = BehaviorTriggerEngine::selectEntry(trigger, event.randomValue);
        if (entry.type == "pool" && !entry.poolId.isEmpty()) {
            appendIfPlayable(requests, ActionRequest::actionPool(entry.poolId));
        } else if (entry.type == "recipe" && !entry.recipeId.isEmpty()) {
            appendIfPlayable(requests, ActionRequest::recipe(entry.recipeId));
        } else if (entry.type == "action" && !entry.actionId.isEmpty()) {
            appendIfPlayable(requests, ActionRequest::action(entry.actionId));
        }
        return requests;
    }

    case PetEventType::PropClicked:
        if (snapshot.currentPropVisible && !snapshot.currentPropClickedRecipeId.isEmpty()) {
            requests.append(ActionRequest::recipe(snapshot.currentPropClickedRecipeId).withHiddenCurrentProp());
        }
        return requests;

    case PetEventType::PropExpired:
        if (snapshot.currentPropVisible && !snapshot.currentPropExpiredRecipeId.isEmpty()) {
            requests.append(ActionRequest::recipe(snapshot.currentPropExpiredRecipeId).withHiddenCurrentProp());
        }
        return requests;
    }

    return requests;
}
