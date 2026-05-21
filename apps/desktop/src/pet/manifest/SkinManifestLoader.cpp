#include "pet/manifest/SkinManifestLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStandardPaths>

namespace {
constexpr auto kFallbackActionId = "idle_stand";
constexpr auto kFallbackAnimationUrl = "qrc:/pet/stand-right.gif";

struct LoadContext
{
    QString sourceName;
    QUrl rootUrl;
    QString skinId;
    QString skinName;
    bool builtin = false;
};

void resolveManifestUrls(SkinManifest &manifest, const QUrl &rootUrl);

SkinManifest parseManifestDocument(const QJsonDocument &document, const LoadContext &context);

SkinManifest loadManifestFile(const QString &path, const LoadContext &context)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    return parseManifestDocument(document, context);
}

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
    if (object.contains("pool")) {
        return ActionRequest::actionPool(object.value("pool").toString());
    }
    if (object.contains("recipe")) {
        return ActionRequest::recipe(object.value("recipe").toString());
    }
    if (object.contains("action")) {
        return ActionRequest::action(object.value("action").toString());
    }

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

ClickBehaviorEntry clickBehaviorEntryFromJsonValue(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();

    ClickBehaviorEntry entry;
    entry.zoneId = object.value("zone").toString();
    entry.when = object.value("when").toString("default");
    entry.customInteractionId = object.value("customInteraction").toString();
    entry.request = requestFromJsonObject(object);

    return entry;
}

BehaviorRuleCondition behaviorRuleConditionFromJsonObject(const QJsonObject &object)
{
    BehaviorRuleCondition condition;
    condition.actionId = object.value("action").toString();
    if (object.contains("holdCompleted")) {
        condition.hasHoldCompleted = true;
        condition.holdCompleted = object.value("holdCompleted").toBool(false);
    }
    return condition;
}

QStringList stringListFromJsonArray(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array) {
        const QString item = value.toString().trimmed();
        if (!item.isEmpty() && !values.contains(item)) {
            values.append(item);
        }
    }
    return values;
}
} // namespace

namespace {
SkinManifest parseManifestDocument(const QJsonDocument &document, const LoadContext &context)
{
    SkinManifest manifest;
    manifest.skinId = context.skinId;
    manifest.skinName = context.skinName;
    manifest.skinRootUrl = context.rootUrl;
    manifest.builtin = context.builtin;

    const QJsonObject root = document.object();
    manifest.fallbackAction = root.value("fallbackAction").toString(kFallbackActionId);
    manifest.defaultFacing = root.value("defaultFacing").toString("right");
    manifest.defaultSizeId = root.value("defaultSize").toString();

    const QJsonObject canvas = root.value("canvas").toObject();
    manifest.canvas.windowSize = canvas.value("windowSize").toDouble(manifest.canvas.windowSize);
    manifest.canvas.imageSize = canvas.value("imageSize").toDouble(manifest.canvas.imageSize);
    manifest.canvas.hitZoneSize = canvas.value("hitZoneSize").toDouble(manifest.canvas.hitZoneSize);
    manifest.canvas.idleLoopActionId = canvas.value("idleLoopAction").toString();

    const QJsonObject audio = root.value("audio").toObject();
    manifest.audio.defaultVoiceLanguage = audio.value("defaultVoiceLanguage").toString();
    const QJsonArray voiceLanguages = audio.value("voiceLanguages").toArray();
    for (const QJsonValue &languageValue : voiceLanguages) {
        AudioLanguageDefinition language;
        if (languageValue.isObject()) {
            const QJsonObject languageObject = languageValue.toObject();
            language.id = languageObject.value("id").toString().trimmed();
            language.label = languageObject.value("label").toString().trimmed();
        } else if (languageValue.isString()) {
            language.id = languageValue.toString().trimmed();
        }

        if (!language.id.isEmpty()) {
            if (language.label.isEmpty()) {
                language.label = language.id;
            }
            manifest.audio.voiceLanguages.append(language);
        }
    }

    const QJsonObject capabilities = root.value("capabilities").toObject();
    const QJsonObject rest = capabilities.value("rest").toObject();
    manifest.capabilities.rest.enterRecipeId = rest.value("enterRecipe").toString();
    manifest.capabilities.rest.exitRecipeId = rest.value("exitRecipe").toString();
    manifest.capabilities.rest.loopActionId = rest.value("loopAction").toString();

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

    const QJsonObject movementFacingMap = root.value("movementFacingMap").toObject();
    for (auto it = movementFacingMap.constBegin(); it != movementFacingMap.constEnd(); ++it) {
        const QString facing = it.value().toString();
        if (!it.key().isEmpty() && !facing.isEmpty()) {
            manifest.movementFacingMap.insert(it.key(), facing);
        }
    }

    const QJsonArray sizes = root.value("sizes").toArray();
    for (const QJsonValue &value : sizes) {
        const QJsonObject sizeObject = value.toObject();
        PetSizeDefinition size;
        size.id = sizeObject.value("id").toString();
        size.label = sizeObject.value("label").toString(size.id);
        size.scale = sizeObject.value("scale").toDouble(0);
        if (!size.id.isEmpty() && size.scale > 0) {
            manifest.sizes.append(size);
        }
    }

    if (manifest.defaultSizeId.isEmpty() && !manifest.sizes.isEmpty()) {
        manifest.defaultSizeId = manifest.sizes.constFirst().id;
    }

    const QJsonArray expressions = root.value("expressions").toArray();
    for (const QJsonValue &value : expressions) {
        const QJsonObject expressionObject = value.toObject();
        ExpressionDefinition expression;
        expression.id = expressionObject.value("id").toString().trimmed();
        expression.label = expressionObject.value("label").toString(expression.id).trimmed();
        expression.description = expressionObject.value("description").toString().trimmed();
        expression.allowedStates = stringListFromJsonArray(expressionObject.value("allowedStates").toArray());
        expression.priority = expressionObject.value("priority").toInt(0);

        if (!expression.id.isEmpty()) {
            manifest.expressions.insert(expression.id, expression);
        }
    }

    const QJsonObject expressionMappings = root.value("expressionMappings").toObject();
    for (auto it = expressionMappings.constBegin(); it != expressionMappings.constEnd(); ++it) {
        const QJsonObject mappingObject = it.value().toObject();
        ExpressionMappingDefinition mapping;
        mapping.selection = mappingObject.value("selection").toString("first_available").trimmed();
        mapping.fallbackExpressionId = mappingObject.value("fallback").toString("neutral").trimmed();

        const QJsonArray actions = mappingObject.value("actions").toArray();
        for (const QJsonValue &actionValue : actions) {
            const QJsonObject actionObject = actionValue.toObject();
            ExpressionMappingEntry entry;
            entry.request = requestFromJsonObject(actionObject);
            entry.allowedStates = stringListFromJsonArray(actionObject.value("allowedStates").toArray());
            entry.weight = actionObject.value("weight").toInt(1);

            if (entry.request.kind != ActionRequestKind::None) {
                mapping.actions.append(entry);
            }
        }

        if (!it.key().isEmpty() && !mapping.actions.isEmpty()) {
            manifest.expressionMappings.insert(it.key(), mapping);
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

    const QJsonObject clickBehaviors = root.value("clickBehaviors").toObject();

    const QJsonArray singleClickBehaviors = clickBehaviors.value("singleClick").toArray();
    for (const QJsonValue &value : singleClickBehaviors) {
        const ClickBehaviorEntry entry = clickBehaviorEntryFromJsonValue(value);
        if (!entry.zoneId.isEmpty()
            && manifest.hitZones.contains(entry.zoneId)
            && entry.request.kind != ActionRequestKind::None) {
            manifest.clickBehaviors.singleClick.append(entry);
        }
    }

    const QJsonArray doubleClickBehaviors = clickBehaviors.value("doubleClick").toArray();
    for (const QJsonValue &value : doubleClickBehaviors) {
        const ClickBehaviorEntry entry = clickBehaviorEntryFromJsonValue(value);
        if (entry.request.kind != ActionRequestKind::None || !entry.customInteractionId.isEmpty()) {
            manifest.clickBehaviors.doubleClick.append(entry);
        }
    }

    const QJsonArray customInteractions = root.value("customInteractions").toArray();
    for (const QJsonValue &value : customInteractions) {
        QString interactionId;
        if (value.isObject()) {
            interactionId = value.toObject().value("id").toString();
        } else {
            interactionId = value.toString();
        }

        if (!interactionId.isEmpty() && !manifest.customInteractions.contains(interactionId)) {
            manifest.customInteractions.append(interactionId);
        }
    }

    const QJsonObject customInteractionConfig = root.value("customInteractionConfig").toObject();
    for (auto it = customInteractionConfig.constBegin(); it != customInteractionConfig.constEnd(); ++it) {
        if (!it.key().isEmpty() && it.value().isObject()) {
            manifest.customInteractionConfigs.insert(it.key(), it.value().toObject().toVariantMap());
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

    const QJsonArray behaviorRules = root.value("behaviorRules").toArray();
    for (const QJsonValue &value : behaviorRules) {
        const QJsonObject ruleObject = value.toObject();

        BehaviorRuleDefinition rule;
        rule.event = ruleObject.value("event").toString();
        rule.when = behaviorRuleConditionFromJsonObject(ruleObject.value("when").toObject());
        rule.request = requestFromJsonObject(ruleObject);

        if (!rule.event.isEmpty() && rule.request.kind != ActionRequestKind::None) {
            manifest.behaviorRules.append(rule);
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
        action.blocksPointerInteraction = actionObject.value("blocksPointerInteraction").toBool(false);
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
            entry.request = requestFromJsonObject(entryObject);
            entry.recipeId = entryObject.value("recipe").toString();
            entry.actionId = entryObject.value("action").toString();
            entry.weight = entryObject.value("weight").toInt(1);
            if (entry.weight < 1) {
                entry.weight = 1;
            }

            if (entry.request.kind != ActionRequestKind::None
                    || !entry.recipeId.isEmpty()
                    || !entry.actionId.isEmpty()) {
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
            entry.when = behaviorRuleConditionFromJsonObject(entryObject.value("when").toObject());
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

    resolveManifestUrls(manifest, context.rootUrl);
    return manifest;
}

void resolveManifestUrls(SkinManifest &manifest, const QUrl &rootUrl)
{
    for (ActionDefinition &action : manifest.actions) {
        for (QUrl &url : action.variants) {
            url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
        }
        for (PhaseDefinition &phase : action.phases) {
            for (QUrl &url : phase.variants) {
                url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
            }
        }
    }

    for (PropDefinition &prop : manifest.props) {
        prop.assetUrl = SkinManifestLoader::resolveSkinUrl(prop.assetUrl.toString(), rootUrl);
    }

    for (RecipeDefinition &recipe : manifest.recipes) {
        recipe.soundUrl = SkinManifestLoader::resolveSkinUrl(recipe.soundUrl.toString(), rootUrl);
        for (QUrl &url : recipe.soundUrls) {
            url = SkinManifestLoader::resolveSkinUrl(url.toString(), rootUrl);
        }
    }
}

SkinDescriptor descriptorFromSkinJson(
    const QString &skinJsonPath,
    const QUrl &rootUrl,
    bool builtin
)
{
    QFile file(skinJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    SkinDescriptor descriptor;
    descriptor.id = object.value(QStringLiteral("id")).toString().trimmed();
    descriptor.name = object.value(QStringLiteral("name")).toString().trimmed();
    descriptor.version = object.value(QStringLiteral("version")).toString().trimmed();
    descriptor.author = object.value(QStringLiteral("author")).toString().trimmed();
    descriptor.license = object.value(QStringLiteral("license")).toString().trimmed();
    descriptor.manifestVersion = object.value(QStringLiteral("manifestVersion")).toInt(1);
    descriptor.minAppVersion = object.value(QStringLiteral("minAppVersion")).toString().trimmed();
    descriptor.rootUrl = rootUrl;
    descriptor.builtin = builtin;
    descriptor.thumbnailUrl = SkinManifestLoader::resolveSkinUrl(
        object.value(QStringLiteral("thumbnail")).toString(),
        rootUrl
    );

    if (descriptor.id.isEmpty()) {
        return {};
    }
    if (descriptor.name.isEmpty()) {
        descriptor.name = descriptor.id;
    }
    return descriptor;
}

bool hasReadableManifest(const QString &manifestPath)
{
    return QFileInfo::exists(manifestPath) && QFileInfo(manifestPath).isFile();
}

void completeFilesystemDescriptor(
    SkinDescriptor &descriptor,
    const QString &manifestPath
)
{
    descriptor.manifestPath = manifestPath;
    if (!hasReadableManifest(descriptor.manifestPath)) {
        descriptor.id.clear();
    }
}
} // namespace

SkinManifest SkinManifestLoader::loadFromResource(const QString &resourcePath)
{
    return loadManifestFile(resourcePath, LoadContext{
        resourcePath,
        QUrl(QStringLiteral("qrc:/skins/miles-edgeworth/")),
        QStringLiteral("miles-edgeworth"),
        QStringLiteral("御剑怜侍"),
        true,
    });
}

SkinManifest SkinManifestLoader::loadFromDescriptor(const SkinDescriptor &descriptor)
{
    SkinManifest manifest = loadManifestFile(descriptor.manifestPath, LoadContext{
        descriptor.manifestPath,
        descriptor.rootUrl,
        descriptor.id,
        descriptor.name,
        descriptor.builtin,
    });
    if (manifest.skinName.isEmpty()) {
        manifest.skinName = descriptor.id;
    }
    return manifest;
}

SkinManifest SkinManifestLoader::loadFromDirectory(const QString &filesystemPath)
{
    QList<SkinDescriptor> descriptors = discoverInDirectories(QStringList{filesystemPath}, false);
    if (descriptors.isEmpty()) {
        return {};
    }
    return loadFromDescriptor(descriptors.first());
}

SkinManifest SkinManifestLoader::fallbackManifest()
{
    SkinManifest manifest;
    manifest.fallbackAction = kFallbackActionId;
    manifest.canvas.windowSize = 120.0;
    manifest.canvas.imageSize = 100.0;
    manifest.canvas.hitZoneSize = 240.0;
    manifest.canvas.idleLoopActionId = kFallbackActionId;
    manifest.defaultSizeId = "medium";
    manifest.audio.defaultVoiceLanguage = "jp";
    manifest.audio.voiceLanguages.append(AudioLanguageDefinition {"jp", QStringLiteral("日语")});
    manifest.expressions.insert("neutral", ExpressionDefinition {"neutral", QStringLiteral("默认"), QStringLiteral("默认站立表达"), {}, 0});
    ExpressionMappingDefinition neutralMapping;
    neutralMapping.selection = "first_available";
    neutralMapping.fallbackExpressionId.clear();
    neutralMapping.actions.append(ExpressionMappingEntry {ActionRequest::action(kFallbackActionId), {}, 1});
    manifest.expressionMappings.insert("neutral", neutralMapping);
    manifest.sizes.append(PetSizeDefinition {"medium", QStringLiteral("中"), 2.0});
    manifest.facings = {"right", "left"};
    manifest.movementDirections = {"east", "west", "northEast", "northWest", "southEast", "southWest", "north", "south"};
    manifest.defaultFacing = "right";
    manifest.movementFacingMap.insert("east", "right");
    manifest.movementFacingMap.insert("northEast", "right");
    manifest.movementFacingMap.insert("southEast", "right");
    manifest.movementFacingMap.insert("west", "left");
    manifest.movementFacingMap.insert("northWest", "left");
    manifest.movementFacingMap.insert("southWest", "left");
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

QList<SkinDescriptor> SkinManifestLoader::discoverAll()
{
    return discoverInDirectories(
        QStringList{userSkinDirectoryPath(), portableSkinDirectoryPath()},
        true
    );
}

QList<SkinDescriptor> SkinManifestLoader::discoverInDirectories(const QStringList &directories, bool includeBuiltins)
{
    QList<SkinDescriptor> result;
    QSet<QString> seenIds;

    auto appendDescriptor = [&](const SkinDescriptor &descriptor) {
        if (descriptor.id.isEmpty() || seenIds.contains(descriptor.id)) {
            return;
        }
        seenIds.insert(descriptor.id);
        result.append(descriptor);
    };

    for (const QString &directoryPath : directories) {
        QDir root(directoryPath);
        if (!root.exists()) {
            continue;
        }

        const QFileInfo rootSkinJson(root.filePath(QStringLiteral("skin.json")));
        if (rootSkinJson.exists()) {
            SkinDescriptor descriptor = descriptorFromSkinJson(
                rootSkinJson.absoluteFilePath(),
                QUrl::fromLocalFile(root.absolutePath() + QLatin1Char('/')),
                false
            );
            completeFilesystemDescriptor(descriptor, root.filePath(QStringLiteral("manifest.json")));
            if (!descriptor.id.isEmpty()) {
                appendDescriptor(descriptor);
                continue;
            }
        }

        const QFileInfoList children = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &child : children) {
            const QDir childDir(child.absoluteFilePath());
            const QString skinJsonPath = childDir.filePath(QStringLiteral("skin.json"));
            if (!QFileInfo::exists(skinJsonPath)) {
                continue;
            }
            SkinDescriptor descriptor = descriptorFromSkinJson(
                skinJsonPath,
                QUrl::fromLocalFile(child.absoluteFilePath() + QLatin1Char('/')),
                false
            );
            completeFilesystemDescriptor(descriptor, childDir.filePath(QStringLiteral("manifest.json")));
            appendDescriptor(descriptor);
        }
    }

    if (includeBuiltins) {
        SkinDescriptor miles;
        miles.id = QStringLiteral("miles-edgeworth");
        miles.name = QStringLiteral("御剑怜侍");
        miles.version = QStringLiteral("0.2.0");
        miles.author = QStringLiteral("tiantian180");
        miles.license = QStringLiteral("fan-project");
        miles.manifestVersion = 1;
        miles.minAppVersion = QStringLiteral("0.2.0");
        miles.rootUrl = QUrl(QStringLiteral("qrc:/skins/miles-edgeworth/"));
        miles.thumbnailUrl = resolveSkinUrl(QStringLiteral("skin:assets/body/idle/stand-right.gif"), miles.rootUrl);
        miles.manifestPath = QStringLiteral(":/skins/miles-edgeworth/manifest.json");
        miles.builtin = true;
        appendDescriptor(miles);
    }

    return result;
}

QString SkinManifestLoader::userSkinDirectoryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/skins");
}

QString SkinManifestLoader::portableSkinDirectoryPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/skins");
}

QUrl SkinManifestLoader::resolveSkinUrl(const QString &rawUrl, const QUrl &rootUrl)
{
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.scheme() != QStringLiteral("skin")) {
        return url;
    }

    QString relativePath = trimmed.mid(QStringLiteral("skin:").size());
    while (relativePath.startsWith(QLatin1Char('/'))) {
        relativePath.remove(0, 1);
    }
    if (relativePath.isEmpty() || relativePath.contains(QStringLiteral(".."))) {
        return {};
    }

    QUrl resolved = rootUrl;
    QString base = resolved.path();
    if (!base.endsWith(QLatin1Char('/'))) {
        base.append(QLatin1Char('/'));
    }
    resolved.setPath(base + relativePath);
    return resolved;
}
