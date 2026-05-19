#pragma once

#include "pet/commands/SkinCommand.h"
#include "pet/requests/ActionRequest.h"

#include <QHash>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QUrl>

// SkinManifest 保存皮肤包 manifest.json 解析后的结构化数据。
//
// 这里不放播放状态、随机数、计时器或 QML 交互逻辑，只表达“皮肤提供了什么”：
// 有哪些动作、动作如何映射到资源、哪些候选池可随机抽取，以及哪些区域能响应点击。
// 运行时后续会围绕这个数据对象继续拆出 Loader、Selector 和调度器。
struct PhaseDefinition
{
    QString loopMode = "loop";
    QString nextPhase;
    QHash<QString, QUrl> variants;
};

struct ActionDefinition
{
    QString label;
    QString category;
    QString loopMode = "loop";
    int priority = 0;
    bool blocksPointerInteraction = false;
    QStringList tags;
    QHash<QString, QUrl> variants;
    QHash<QString, QPointF> movementDeltas;
    QHash<QString, QString> facingAfter;
    QString initialPhase;
    QString exitPhase;
    QHash<QString, PhaseDefinition> phases;
};

struct RecipeStep
{
    QString actionId;
    QString phaseId;
    QString recipeId;
    QString movementDirection;
    QString facing;
    int repeat = 1;
    int durationMs = 0;
};

struct RecipeDefinition
{
    QString label;
    QString scope;
    QString actionId;
    QUrl soundUrl;
    QHash<QString, QUrl> soundUrls;
    QString propId;
    QList<RecipeStep> steps;
};

struct PropDefinition
{
    QString id;
    QUrl assetUrl;
    double width = 0;
    double height = 0;
    double visualWidth = 0;
    double visualHeight = 0;
    int delayMs = 0;
    int durationMs = 0;
    QString clickedRecipeId;
    QString expiredRecipeId;
    QHash<QString, QPointF> startOffsets;
    QHash<QString, QPointF> travelDeltas;
    QHash<QString, QPointF> travelBaseDeltas;
    QHash<QString, QPointF> travelPerScaleDeltas;
};

struct ActionPoolEntry
{
    QString recipeId;
    QString actionId;
    int weight = 1;
};

struct ActionPoolDefinition
{
    QString label;
    QList<ActionPoolEntry> entries;
};

struct BehaviorRuleCondition
{
    QString actionId;
    bool hasHoldCompleted = false;
    bool holdCompleted = false;
};

struct BehaviorTriggerEntry
{
    QString type;
    BehaviorRuleCondition when;
    QString poolId;
    QString recipeId;
    QString actionId;
    int weight = 1;
};

struct BehaviorTriggerDefinition
{
    QString label;
    QString state;
    QString actionId;
    bool requiresNoActiveRecipe = false;
    QList<BehaviorTriggerEntry> entries;
};

struct BehaviorRuleDefinition
{
    QString event;
    BehaviorRuleCondition when;
    ActionRequest request;
};

struct RestCapabilityDefinition
{
    QString enterRecipeId;
    QString exitRecipeId;
    QString loopActionId;

    bool enabled() const
    {
        return !enterRecipeId.isEmpty() && !loopActionId.isEmpty();
    }
};

struct CapabilityDefinition
{
    RestCapabilityDefinition rest;
};

struct HitZoneDefinition
{
    QString id;
    QRectF rect;
    QList<QPointF> polygon;
    QHash<QString, QRectF> facingRects;
    QHash<QString, QList<QPointF>> facingPolygons;
};

struct ClickBehaviorEntry
{
    QString zoneId;
    QString when = "default";
    ActionRequest request;
    QString customInteractionId;
};

struct ClickBehaviorDefinition
{
    QList<ClickBehaviorEntry> singleClick;
    QList<ClickBehaviorEntry> doubleClick;
};

struct SkinManifest
{
    CapabilityDefinition capabilities;
    QHash<QString, QString> stateToAction;
    QHash<QString, ActionDefinition> actions;
    QHash<QString, RecipeDefinition> recipes;
    QHash<QString, ActionPoolDefinition> actionPools;
    QHash<QString, BehaviorTriggerDefinition> behaviorTriggers;
    QList<BehaviorRuleDefinition> behaviorRules;
    QHash<QString, PropDefinition> props;
    QHash<QString, HitZoneDefinition> hitZones;
    QHash<QString, SkinCommandDefinition> skinCommands;
    ClickBehaviorDefinition clickBehaviors;
    QHash<QString, QString> movementFacingMap;
    QString fallbackAction = "idle_stand";
    QStringList facings = {"right", "left"};
    QStringList movementDirections;
    QString defaultFacing = "right";
};
