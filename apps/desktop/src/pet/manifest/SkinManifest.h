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
#include <QVariantMap>

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
    ActionRequest request;
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

struct CanvasDefinition
{
    // windowSize / imageSize 是 scale=1 时的基础尺寸。
    // Miles 当前中号 scale=2，因此窗口 120*2=240，动画 100*2=200。
    double windowSize = 120.0;
    double imageSize = 100.0;
    // hitZoneSize 是点击命中区域使用的逻辑画布尺寸。它独立于显示窗口尺寸，
    // 因为有些皮肤会按放大后的像素坐标绘制 hitZones。
    double hitZoneSize = 0.0;
    QString idleLoopActionId;
};

struct PetSizeDefinition
{
    QString id;
    QString label;
    double scale = 1.0;
};

struct AudioLanguageDefinition
{
    QString id;
    QString label;
};

struct AudioDefinition
{
    QString defaultVoiceLanguage;
    QList<AudioLanguageDefinition> voiceLanguages;
};

struct ExpressionDefinition
{
    QString id;
    QString label;
    QString description;
    QStringList allowedStates;
    int priority = 0;
};

struct ExpressionMappingEntry
{
    ActionRequest request;
    QStringList allowedStates;
    int weight = 1;
};

struct ExpressionMappingDefinition
{
    QString selection = "first_available";
    QString fallbackExpressionId = "neutral";
    QList<ExpressionMappingEntry> actions;
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
    CanvasDefinition canvas;
    AudioDefinition audio;
    CapabilityDefinition capabilities;
    QList<PetSizeDefinition> sizes;
    QString defaultSizeId;
    QHash<QString, QString> stateToAction;
    QHash<QString, ActionDefinition> actions;
    QHash<QString, RecipeDefinition> recipes;
    QHash<QString, ActionPoolDefinition> actionPools;
    QHash<QString, ExpressionDefinition> expressions;
    QHash<QString, ExpressionMappingDefinition> expressionMappings;
    QHash<QString, BehaviorTriggerDefinition> behaviorTriggers;
    QList<BehaviorRuleDefinition> behaviorRules;
    QHash<QString, PropDefinition> props;
    QHash<QString, HitZoneDefinition> hitZones;
    QHash<QString, SkinCommandDefinition> skinCommands;
    ClickBehaviorDefinition clickBehaviors;
    QStringList customInteractions;
    QHash<QString, QVariantMap> customInteractionConfigs;
    QHash<QString, QString> movementFacingMap;
    QString fallbackAction = "idle_stand";
    QStringList facings = {"right", "left"};
    QStringList movementDirections;
    QString defaultFacing = "right";
};
