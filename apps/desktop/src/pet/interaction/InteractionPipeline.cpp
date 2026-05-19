#include "pet/interaction/InteractionPipeline.h"

#include "pet/behavior/BehaviorTriggerEngine.h"
#include "pet/commands/SkinCommandResolver.h"
#include "pet/interaction/CustomInteractionRegistry.h"
#include "pet/interaction/HitZoneMatcher.h"

namespace {
constexpr auto kSleepToggleCommandId = "runtime.sleep.toggle";
constexpr auto kReturnToIdleCommandId = "runtime.returnToIdle";
constexpr auto kFacingToggleCommandId = "runtime.facing.toggle";
constexpr auto kDragShakeEventName = "pointer.dragShake";
constexpr auto kDragReleasedEventName = "pointer.dragReleased";
constexpr auto kIdleLoopFinishedTriggerId = "idle.loopFinished";
constexpr auto kRuntimeStartedTriggerId = "runtime.started";
constexpr auto kActionCompletedTriggerId = "action.completed";

void appendIfPlayable(QList<ActionRequest> &requests, const ActionRequest &request)
{
    if (request.kind != ActionRequestKind::None) {
        requests.append(request);
    }
}

QStringList singleClickZoneIds(const QList<ClickBehaviorEntry> &entries)
{
    // 命中检测只需要 zone 顺序。pool / recipe / action 的选择留在
    // InteractionPipeline 里完成，避免 HitZoneMatcher 知道皮肤命名。
    QStringList zoneIds;
    for (const ClickBehaviorEntry &entry : entries) {
        if (!entry.zoneId.isEmpty()) {
            zoneIds.append(entry.zoneId);
        }
    }
    return zoneIds;
}

QString behaviorRuleEventName(const PetEvent &event)
{
    switch (event.type) {
    case PetEventType::PointerDragShake:
        return QString::fromUtf8(kDragShakeEventName);
    case PetEventType::PointerDragReleased:
        return QString::fromUtf8(kDragReleasedEventName);
    default:
        return {};
    }
}

QString behaviorTriggerIdForEvent(const PetEvent &event)
{
    switch (event.type) {
    case PetEventType::IdleLoopFinished:
        return QString::fromUtf8(kIdleLoopFinishedTriggerId);
    case PetEventType::RuntimeStarted:
        return QString::fromUtf8(kRuntimeStartedTriggerId);
    case PetEventType::ActionCompleted:
        return QString::fromUtf8(kActionCompletedTriggerId);
    default:
        return {};
    }
}

bool behaviorRuleMatches(
    const BehaviorRuleDefinition &rule,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    if (rule.event != behaviorRuleEventName(event)) {
        return false;
    }

    if (!rule.when.actionId.isEmpty() && rule.when.actionId != snapshot.currentActionId) {
        return false;
    }

    if (rule.when.hasHoldCompleted && rule.when.holdCompleted != event.dragHoldCompleted) {
        return false;
    }

    return true;
}

void appendBehaviorRuleRequests(
    QList<ActionRequest> &requests,
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    // behaviorRules 是皮肤包把通用事件映射到播放请求的第一版机制。
    // 这里不理解 Miles 的动作名，只按 event/when 条件挑选声明好的请求。
    for (const BehaviorRuleDefinition &rule : manifest.behaviorRules) {
        if (behaviorRuleMatches(rule, snapshot, event)) {
            appendIfPlayable(requests, rule.request);
        }
    }
}

void appendBehaviorTriggerRequests(
    QList<ActionRequest> &requests,
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    const QString triggerId = behaviorTriggerIdForEvent(event);
    if (triggerId.isEmpty() || !manifest.behaviorTriggers.contains(triggerId)) {
        return;
    }

    const BehaviorTriggerDefinition trigger = manifest.behaviorTriggers.value(triggerId);
    const BehaviorTriggerContext context {
        snapshot.currentState,
        snapshot.currentActionId,
        !snapshot.currentRecipeId.isEmpty(),
    };

    if (!BehaviorTriggerEngine::matches(trigger, context)) {
        return;
    }

    const BehaviorTriggerEntry entry = BehaviorTriggerEngine::selectEntry(trigger, context, event.randomValue);
    if (entry.type == "pool" && !entry.poolId.isEmpty()) {
        appendIfPlayable(requests, ActionRequest::actionPool(entry.poolId));
    } else if (entry.type == "recipe" && !entry.recipeId.isEmpty()) {
        appendIfPlayable(requests, ActionRequest::recipe(entry.recipeId));
    } else if (entry.type == "action" && !entry.actionId.isEmpty()) {
        appendIfPlayable(requests, ActionRequest::action(entry.actionId));
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
        if (!snapshot.pointerInteractionEnabled || snapshot.sleeping || snapshot.sleepTransitioning) {
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
        const QString zoneId = HitZoneMatcher::hitZoneIdForPoint(
            manifest,
            hitZoneContext,
            singleClickZoneIds(manifest.clickBehaviors.singleClick),
            event.x,
            event.y,
            event.width,
            event.height
        );
        // 同一个 zone 可以在不同皮肤里绑定到不同请求；这里按 manifest
        // 声明顺序选择第一条命中的行为，保持旧版分区优先级。
        for (const ClickBehaviorEntry &entry : manifest.clickBehaviors.singleClick) {
            if (entry.zoneId == zoneId) {
                appendIfPlayable(requests, entry.request);
                break;
            }
        }
        return requests;
    }

    case PetEventType::PointerDoubleClick:
        if (!snapshot.pointerInteractionEnabled) {
            return requests;
        }
        if (snapshot.sleepTransitioning) {
            return requests;
        }
        if (snapshot.sleeping) {
            requests.append(ActionRequest::returnToIdle());
            return requests;
        }
        // 默认双击行为也从 manifest 读取。后续 Custom Interaction 接入后，
        // 可以在同一个列表里先声明高级交互，再声明普通 action pool 兜底。
        for (const ClickBehaviorEntry &entry : manifest.clickBehaviors.doubleClick) {
            if (entry.when.isEmpty() || entry.when == "default") {
                appendIfPlayable(requests, entry.request);
                break;
            }
        }
        return requests;

    case PetEventType::PointerDragShake:
    case PetEventType::PointerDragReleased:
        if (!snapshot.pointerInteractionEnabled || snapshot.sleeping || snapshot.sleepTransitioning) {
            return requests;
        }
        appendBehaviorRuleRequests(requests, manifest, snapshot, event);
        return requests;

    case PetEventType::MenuCommand:
        if (event.commandId == QString::fromUtf8(kSleepToggleCommandId)) {
            const bool restCapabilityReady = !manifest.capabilities.rest.enterRecipeId.isEmpty()
                && !manifest.capabilities.rest.loopActionId.isEmpty();
            if (!restCapabilityReady) {
                return requests;
            }
            if (snapshot.sleepTransitioning) {
                return requests;
            }
            if (snapshot.sleeping) {
                if (!manifest.capabilities.rest.exitRecipeId.isEmpty()) {
                    requests.append(ActionRequest::recipe(manifest.capabilities.rest.exitRecipeId));
                } else {
                    requests.append(ActionRequest::returnToIdle());
                }
            } else {
                requests.append(ActionRequest::recipe(manifest.capabilities.rest.enterRecipeId));
            }
        } else if (event.commandId == QString::fromUtf8(kReturnToIdleCommandId)) {
            requests.append(ActionRequest::returnToIdle());
        } else if (event.commandId == QString::fromUtf8(kFacingToggleCommandId)) {
            requests.append(ActionRequest::toggleFacing());
        } else {
            appendIfPlayable(requests, SkinCommandResolver::resolveCommand(manifest, snapshot, event.commandId));
        }
        return requests;

    case PetEventType::IdleLoopFinished:
    case PetEventType::RuntimeStarted:
    case PetEventType::ActionCompleted:
        appendBehaviorTriggerRequests(requests, manifest, snapshot, event);
        return requests;

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
