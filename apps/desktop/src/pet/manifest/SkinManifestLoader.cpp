#include "pet/manifest/SkinManifestLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr auto kFallbackActionId = "idle_stand";
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";

QRectF rectFromJsonObject(const QJsonObject &object)
{
    return QRectF(
        object.value("x").toDouble(0),
        object.value("y").toDouble(0),
        object.value("width").toDouble(0),
        object.value("height").toDouble(0)
    );
}

QList<QPointF> polygonFromJsonArray(const QJsonArray &array)
{
    QList<QPointF> polygon;
    for (const QJsonValue &value : array) {
        const QJsonObject pointObject = value.toObject();
        if (pointObject.contains("x") && pointObject.contains("y")) {
            polygon.append(QPointF(pointObject.value("x").toDouble(0), pointObject.value("y").toDouble(0)));
        }
    }
    return polygon;
}

ActionRequest requestFromJsonObject(const QJsonObject &object)
{
    const QString type = object.value("type").toString();
    if (type == "pool") {
        return ActionRequest::actionPool(object.value("pool").toString());
    }
    if (type == "recipe") {
        return ActionRequest::recipe(object.value("recipe").toString());
    }
    if (type == "action") {
        return ActionRequest::action(object.value("action").toString());
    }
    if (type == "returnToIdle") {
        return ActionRequest::returnToIdle();
    }
    if (type == "toggleFacing") {
        return ActionRequest::toggleFacing();
    }
    return ActionRequest::none();
}
} // namespace

SkinManifest SkinManifestLoader::loadFromResource(const QString &resourcePath)
{
    SkinManifest manifest;

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return manifest;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return manifest;
    }

    const QJsonObject root = document.object();
    manifest.fallbackAction = root.value("fallbackAction").toString(kFallbackActionId);
    manifest.defaultFacing = root.value("defaultFacing").toString("right");

    const QJsonArray facings = root.value("facings").toArray();
    if (!facings.isEmpty()) {
        manifest.facings.clear();
        for (const QJsonValue &value : facings) {
            const QString facing = value.toString();
            if (!facing.isEmpty()) {
                manifest.facings.append(facing);
            }
        }
    }

    const QJsonArray movementDirections = root.value("movementDirections").toArray();
    if (!movementDirections.isEmpty()) {
        manifest.movementDirections.clear();
        for (const QJsonValue &value : movementDirections) {
            const QString movementDirection = value.toString();
            if (!movementDirection.isEmpty()) {
                manifest.movementDirections.append(movementDirection);
            }
        }
    }

    const QJsonObject hitZones = root.value("hitZones").toObject();
    for (auto it = hitZones.constBegin(); it != hitZones.constEnd(); ++it) {
        const QJsonObject zoneObject = it.value().toObject();
        const QString zoneType = zoneObject.value("type").toString("rect");
        if (zoneType != "rect" && zoneType != "polygon") {
            continue;
        }

        HitZoneDefinition zone;
        zone.id = it.key();
        zone.rect = rectFromJsonObject(zoneObject);
        zone.polygon = polygonFromJsonArray(zoneObject.value("polygon").toArray());

        const QJsonObject zoneVariants = zoneObject.value("variants").toObject();
        for (auto variantIt = zoneVariants.constBegin(); variantIt != zoneVariants.constEnd(); ++variantIt) {
            const QJsonObject variantObject = variantIt.value().toObject();
            const QRectF variantRect = rectFromJsonObject(variantObject);
            if (variantRect.isValid()) {
                zone.facingRects.insert(variantIt.key(), variantRect);
            }

            const QList<QPointF> variantPolygon = polygonFromJsonArray(variantObject.value("polygon").toArray());
            if (variantPolygon.size() >= 3) {
                zone.facingPolygons.insert(variantIt.key(), variantPolygon);
            }
        }

        if (zone.rect.isValid() || zone.polygon.size() >= 3 || !zone.facingRects.isEmpty() || !zone.facingPolygons.isEmpty()) {
            manifest.hitZones.insert(zone.id, zone);
        }
    }

    const QJsonArray singleClickPools = root.value("clickBehaviors").toObject().value("singleClick").toArray();
    for (const QJsonValue &value : singleClickPools) {
        const QString poolId = value.toString();
        if (!poolId.isEmpty()) {
            manifest.singleClickPools.append(poolId);
        }
    }

    const QJsonObject skinCommands = root.value("skinCommands").toObject();
    for (auto it = skinCommands.constBegin(); it != skinCommands.constEnd(); ++it) {
        const QJsonObject commandObject = it.value().toObject();

        SkinCommandDefinition command;
        command.id = it.key();
        command.label = commandObject.value("label").toString(it.key());
        command.disabledWhenActionId = commandObject
            .value("enabledWhen").toObject()
            .value("notAction").toString();
        command.request = requestFromJsonObject(commandObject.value("request").toObject());

        if (!command.id.isEmpty() && command.request.kind != ActionRequestKind::None) {
            manifest.skinCommands.insert(command.id, command);
        }
    }

    const QJsonObject props = root.value("props").toObject();
    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        const QJsonObject propObject = it.value().toObject();

        PropDefinition prop;
        prop.id = it.key();
        prop.assetUrl = QUrl(propObject.value("asset").toString());
        prop.width = propObject.value("width").toDouble(0);
        prop.height = propObject.value("height").toDouble(0);
        prop.visualWidth = propObject.value("visualWidth").toDouble(0);
        prop.visualHeight = propObject.value("visualHeight").toDouble(0);
        prop.delayMs = propObject.value("delayMs").toInt(0);
        prop.durationMs = propObject.value("durationMs").toInt(0);
        prop.clickedRecipeId = propObject.value("clickedRecipe").toString();
        prop.expiredRecipeId = propObject.value("expiredRecipe").toString();

        const QJsonObject startOffsets = propObject.value("startOffsets").toObject();
        for (auto offsetIt = startOffsets.constBegin(); offsetIt != startOffsets.constEnd(); ++offsetIt) {
            const QJsonObject offsetObject = offsetIt.value().toObject();
            prop.startOffsets.insert(offsetIt.key(), QPointF(offsetObject.value("x").toDouble(0), offsetObject.value("y").toDouble(0)));
        }

        const QJsonObject travel = propObject.value("travel").toObject();
        for (auto travelIt = travel.constBegin(); travelIt != travel.constEnd(); ++travelIt) {
            const QJsonObject travelObject = travelIt.value().toObject();
            prop.travelDeltas.insert(travelIt.key(), QPointF(travelObject.value("x").toDouble(0), travelObject.value("y").toDouble(0)));
        }

        const QJsonObject travelBase = propObject.value("travelBase").toObject();
        for (auto travelIt = travelBase.constBegin(); travelIt != travelBase.constEnd(); ++travelIt) {
            const QJsonObject travelObject = travelIt.value().toObject();
            prop.travelBaseDeltas.insert(travelIt.key(), QPointF(travelObject.value("x").toDouble(0), travelObject.value("y").toDouble(0)));
        }

        const QJsonObject travelPerScale = propObject.value("travelPerScale").toObject();
        for (auto travelIt = travelPerScale.constBegin(); travelIt != travelPerScale.constEnd(); ++travelIt) {
            const QJsonObject travelObject = travelIt.value().toObject();
            prop.travelPerScaleDeltas.insert(travelIt.key(), QPointF(travelObject.value("x").toDouble(0), travelObject.value("y").toDouble(0)));
        }

        if (!prop.id.isEmpty() && !prop.assetUrl.isEmpty()) {
            manifest.props.insert(prop.id, prop);
        }
    }

    const QJsonObject states = root.value("states").toObject();
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        const QString action = it.value().toObject().value("action").toString();
        if (!action.isEmpty()) {
            manifest.stateToAction.insert(it.key(), action);
        }
    }

    const QJsonObject actions = root.value("actions").toObject();
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QJsonObject actionObject = it.value().toObject();

        ActionDefinition action;
        action.label = actionObject.value("label").toString(it.key());
        action.category = actionObject.value("category").toString();
        action.loopMode = actionObject.value("loopMode").toString(actionObject.value("loop").toBool(true) ? "loop" : "onceThenIdle");
        action.priority = actionObject.value("priority").toInt(0);
        action.initialPhase = actionObject.value("initialPhase").toString();
        action.exitPhase = actionObject.value("exitPhase").toString();

        const QJsonArray tags = actionObject.value("tags").toArray();
        for (const QJsonValue &tag : tags) {
            const QString tagText = tag.toString();
            if (!tagText.isEmpty()) {
                action.tags.append(tagText);
            }
        }

        const QJsonObject variants = actionObject.value("variants").toObject();
        for (auto variantIt = variants.constBegin(); variantIt != variants.constEnd(); ++variantIt) {
            const QJsonObject variantObject = variantIt.value().toObject();
            const QString animation = variantObject.value("animation").toString();
            if (!animation.isEmpty()) {
                action.variants.insert(variantIt.key(), QUrl(animation));
            }

            const QJsonObject movementObject = variantObject.value("movement").toObject();
            if (movementObject.contains("dx") || movementObject.contains("dy")) {
                action.movementDeltas.insert(
                    variantIt.key(),
                    QPointF(movementObject.value("dx").toDouble(0), movementObject.value("dy").toDouble(0))
                );
            }
        }

        const QJsonObject facingAfter = actionObject.value("facingAfter").toObject();
        for (auto facingIt = facingAfter.constBegin(); facingIt != facingAfter.constEnd(); ++facingIt) {
            const QString nextFacing = facingIt.value().toString();
            if (!nextFacing.isEmpty()) {
                action.facingAfter.insert(facingIt.key(), nextFacing);
            }
        }

        // 兼容 Phase 0.5 的旧 manifest：如果 action 直接写 animation，
        // 就把它当成默认朝向的 variant。
        const QString legacyAnimation = actionObject.value("animation").toString();
        if (!legacyAnimation.isEmpty()) {
            action.variants.insert(manifest.defaultFacing, QUrl(legacyAnimation));
        }

        const QJsonObject phases = actionObject.value("phases").toObject();
        for (auto phaseIt = phases.constBegin(); phaseIt != phases.constEnd(); ++phaseIt) {
            const QJsonObject phaseObject = phaseIt.value().toObject();
            PhaseDefinition phase;
            phase.loopMode = phaseObject.value("loopMode").toString("loop");
            phase.nextPhase = phaseObject.value("nextPhase").toString();

            const QJsonObject phaseVariants = phaseObject.value("variants").toObject();
            for (auto variantIt = phaseVariants.constBegin(); variantIt != phaseVariants.constEnd(); ++variantIt) {
                const QString animation = variantIt.value().toObject().value("animation").toString();
                if (!animation.isEmpty()) {
                    phase.variants.insert(variantIt.key(), QUrl(animation));
                }
            }

            if (!phase.variants.isEmpty()) {
                action.phases.insert(phaseIt.key(), phase);
            }
        }

        if (!action.variants.isEmpty() || !action.phases.isEmpty()) {
            manifest.actions.insert(it.key(), action);
        }
    }

    const QJsonObject recipes = root.value("recipes").toObject();
    for (auto it = recipes.constBegin(); it != recipes.constEnd(); ++it) {
        const QJsonObject recipeObject = it.value().toObject();

        RecipeDefinition recipe;
        recipe.label = recipeObject.value("label").toString(it.key());
        recipe.scope = recipeObject.value("scope").toString();
        recipe.actionId = recipeObject.value("action").toString();
        recipe.soundUrl = QUrl(recipeObject.value("sound").toString());
        recipe.propId = recipeObject.value("prop").toString();

        const QJsonObject sounds = recipeObject.value("sounds").toObject();
        for (auto soundIt = sounds.constBegin(); soundIt != sounds.constEnd(); ++soundIt) {
            const QString soundUrl = soundIt.value().toString();
            if (!soundUrl.isEmpty()) {
                recipe.soundUrls.insert(soundIt.key(), QUrl(soundUrl));
            }
        }

        const QJsonArray steps = recipeObject.value("steps").toArray();
        for (const QJsonValue &stepValue : steps) {
            const QJsonObject stepObject = stepValue.toObject();

            RecipeStep step;
            step.actionId = stepObject.value("action").toString(recipe.actionId);
            step.phaseId = stepObject.value("phase").toString();
            step.recipeId = stepObject.value("recipe").toString();
            step.movementDirection = stepObject.value("movementDirection").toString(recipeObject.value("movementDirection").toString());
            step.facing = stepObject.value("facing").toString(recipeObject.value("facing").toString());
            step.repeat = stepObject.value("repeat").toInt(1);
            step.durationMs = stepObject.value("durationMs").toInt(0);

            if (!step.actionId.isEmpty() || !step.phaseId.isEmpty() || !step.recipeId.isEmpty()) {
                recipe.steps.append(step);
            }
        }

        // 允许最简单的 recipe 只写 action，不必为了一个动作再包一层 steps。
        if (recipe.steps.isEmpty() && !recipe.actionId.isEmpty()) {
            RecipeStep singleStep;
            singleStep.actionId = recipe.actionId;
            singleStep.movementDirection = recipeObject.value("movementDirection").toString();
            singleStep.facing = recipeObject.value("facing").toString();
            recipe.steps.append(singleStep);
        }

        if (!recipe.steps.isEmpty()) {
            manifest.recipes.insert(it.key(), recipe);
        }
    }

    const QJsonObject actionPools = root.value("actionPools").toObject();
    for (auto it = actionPools.constBegin(); it != actionPools.constEnd(); ++it) {
        const QJsonObject poolObject = it.value().toObject();

        ActionPoolDefinition pool;
        pool.label = poolObject.value("label").toString(it.key());

        const QJsonArray entries = poolObject.value("entries").toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entryObject = entryValue.toObject();

            ActionPoolEntry entry;
            entry.recipeId = entryObject.value("recipe").toString();
            entry.actionId = entryObject.value("action").toString();
            entry.weight = entryObject.value("weight").toInt(1);
            if (entry.weight < 1) {
                entry.weight = 1;
            }

            if (!entry.recipeId.isEmpty() || !entry.actionId.isEmpty()) {
                pool.entries.append(entry);
            }
        }

        if (!pool.entries.isEmpty()) {
            manifest.actionPools.insert(it.key(), pool);
        }
    }

    const QJsonObject behaviorTriggers = root.value("behaviorTriggers").toObject();
    for (auto it = behaviorTriggers.constBegin(); it != behaviorTriggers.constEnd(); ++it) {
        const QJsonObject triggerObject = it.value().toObject();

        BehaviorTriggerDefinition trigger;
        trigger.label = triggerObject.value("label").toString(it.key());

        const QJsonObject whenObject = triggerObject.value("when").toObject();
        trigger.state = whenObject.value("state").toString();
        trigger.actionId = whenObject.value("action").toString();
        trigger.requiresNoActiveRecipe = whenObject.value("requiresNoActiveRecipe").toBool(false);

        const QJsonArray entries = triggerObject.value("entries").toArray();
        for (const QJsonValue &entryValue : entries) {
            const QJsonObject entryObject = entryValue.toObject();

            BehaviorTriggerEntry entry;
            entry.type = entryObject.value("type").toString();
            entry.poolId = entryObject.value("pool").toString();
            entry.recipeId = entryObject.value("recipe").toString();
            entry.actionId = entryObject.value("action").toString();
            entry.weight = entryObject.value("weight").toInt(1);
            if (entry.weight < 1) {
                entry.weight = 1;
            }

            if (!entry.type.isEmpty()) {
                trigger.entries.append(entry);
            }
        }

        if (!trigger.entries.isEmpty()) {
            manifest.behaviorTriggers.insert(it.key(), trigger);
        }
    }

    return manifest;
}

SkinManifest SkinManifestLoader::fallbackManifest()
{
    SkinManifest manifest;
    manifest.fallbackAction = kFallbackActionId;
    manifest.facings = {"right", "left"};
    manifest.movementDirections = {"east", "west", "northEast", "northWest", "southEast", "southWest", "north", "south"};
    manifest.defaultFacing = "right";
    manifest.stateToAction.insert("idle", kFallbackActionId);

    ActionDefinition fallbackAction;
    fallbackAction.loopMode = "loop";
    fallbackAction.variants.insert("right", QUrl(QString::fromUtf8(kFallbackAnimationUrl)));
    manifest.actions.insert(kFallbackActionId, fallbackAction);

    RecipeDefinition idleRecipe;
    idleRecipe.scope = "state";
    idleRecipe.actionId = kFallbackActionId;
    RecipeStep idleStep;
    idleStep.actionId = kFallbackActionId;
    idleRecipe.steps.append(idleStep);
    manifest.recipes.insert("idle.stand", idleRecipe);

    return manifest;
}
