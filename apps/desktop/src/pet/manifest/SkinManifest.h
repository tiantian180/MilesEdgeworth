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

struct AnimationVariant
{
    QUrl url;
    int frameStart = -1;
    int frameEnd = -1;

    bool hasFrameRange() const
    {
        return frameStart >= 0 && frameEnd >= frameStart;
    }
};

struct ClipDefinition
{
    QUrl fileUrl;
    int frameStart = -1;
    int frameEnd = -1;
};

// PhaseDefinition 描述一个 Action 内部的一段独立播放阶段。
// 例如 thinking 动作可以拆成 enter / loop / exit 三个 phase，
// 各自有不同的 GIF 资源、循环模式以及播完后跳转的下一段。
struct PhaseDefinition
{
    QString loopMode = "loop";
    QString nextPhase;
    QHash<QString, AnimationVariant> variants;
};

// ActionDefinition 是一个有语义的动作单元，例如 idle_stand、thinking、walk。
//
// 一个 Action 可能由单个 GIF（variants）组成，也可能拆成多个 phase。
// movementDeltas 用于 walk/run 这类逐帧推进窗口位置的动作；
// facingAfter 描述播完后角色是否改变朝向；blocksPointerInteraction 让
// 类似公文包入场等动画暂时禁用点击。
struct ActionDefinition
{
    QString label;
    QString category;
    QString loopMode = "loop";
    bool blocksPointerInteraction = false;
    QHash<QString, AnimationVariant> variants;
    QHash<QString, QPointF> movementDeltas;
    QHash<QString, QString> facingAfter;
    QString initialPhase;
    QString exitPhase;
    QHash<QString, PhaseDefinition> phases;
};

// RecipeStep 是一条 Recipe 时间线上的单个步骤。
// 可以指定一个 action（或它的某个 phase）、嵌套引用另一个 recipe、
// 临时朝向 / 移动方向，以及播放重复次数 / 时长。
struct RecipeStep
{
    QString actionId;
    QString phaseId;
    QString recipeId;
    QString movementDirection;
    QString facing;
    int repeat = 1;
    int durationMs = 0;
    QString durationMode;
    bool runtimeControlled = false;
};

// RecipeDefinition 是“按时间线把动作和副作用编排起来”的脚本。
// 例如 startup.briefcase（公文包入场）、sleep.enterLoopExit（睡觉的入睡 / 循环 / 醒来）。
// soundUrl / soundUrls 是 recipe 触发时同步播放的音效（后者按语音语言区分）。
// propId 是 recipe 触发的 Prop（如检察官徽章）。
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

// PropDefinition 描述主体动画之外的临时视觉对象（飞出的徽章、掉落物等）。
//
// width/height 是图片原始尺寸；visualWidth/Height 是中号 scale 下的显示尺寸，
// 随当前 petScale 等比例缩放。startOffsets 是不同朝向下的起飞位置，
// travel* 是飞行距离与 scale 的关系（基础 + 每级 scale 增量）。
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

// ActionPoolEntry 是动作池里的一个候选。
// request 形式优先（支持 returnToIdle 等通用请求）；
// recipeId / actionId 是简化写法，向后兼容旧 manifest。
struct ActionPoolEntry
{
    ActionRequest request;
    QString recipeId;
    QString actionId;
    int weight = 1;
};

// ActionPoolDefinition 是一组带权重的候选动作，运行时按权重随机抽取。
// 用于随机 idle、单击分区、双击随机语音动作等场景。
struct ActionPoolDefinition
{
    QString label;
    QList<ActionPoolEntry> entries;
};

// BehaviorRuleCondition 是 behavior 规则的“仅当”过滤条件。
// 例如“仅当当前是 drag_crouch 动作时”“仅当 hold 动画已播完时”。
struct BehaviorRuleCondition
{
    QString actionId;
    bool hasHoldCompleted = false;
    bool holdCompleted = false;
};

// BehaviorTriggerEntry 是 BehaviorTriggerDefinition 里的单个候选。
// type 指明这条候选指向 pool / recipe / action，by weight 随机抽取。
struct BehaviorTriggerEntry
{
    QString type;
    BehaviorRuleCondition when;
    QString poolId;
    QString recipeId;
    QString actionId;
    int weight = 1;
};

// BehaviorTriggerDefinition 把通用事件（如 idle.loopFinished、action.completed）
// 映射成播放请求。例如“idle 循环结束 → 有 70% 概率随机抽取一个 idle.random 池里的动作”。
// state / actionId / requiresNoActiveRecipe 是触发的额外前置条件。
struct BehaviorTriggerDefinition
{
    QString label;
    QString state;
    QString actionId;
    bool requiresNoActiveRecipe = false;
    QList<BehaviorTriggerEntry> entries;
};

// BehaviorRuleDefinition 是对单个 PetEvent 的"事件 → 请求"映射。
// 与 BehaviorTriggerDefinition 不同：这里没有多候选抽取，是固定 1:1 的规则，
// 当前主要用在 pointer.dragShake → drag_crouch 这种确定性映射。
struct BehaviorRuleDefinition
{
    QString event;
    BehaviorRuleCondition when;
    ActionRequest request;
};

// RestCapabilityDefinition 把"睡觉"这种通用能力的入口 / 循环 / 退出 recipe 集中声明。
// 没有 enterRecipe + loopAction 的皮肤视为不具备 rest 能力，菜单不显示对应入口。
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

// CapabilityDefinition 汇总皮肤声明的"可选能力"。
// 当前只有 rest（睡觉），未来可扩展 voice、greeting 等。
struct CapabilityDefinition
{
    RestCapabilityDefinition rest;
};

// CanvasDefinition 声明皮肤的基础逻辑画布。
// 决定窗口尺寸、动画显示尺寸、点击命中坐标系。
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

// PetSizeDefinition 描述一个尺寸档位（迷你 / 小 / 中 / 大）。
// id 用于持久化记住用户偏好，scale 是相对 windowSize/imageSize 的倍数。
struct PetSizeDefinition
{
    QString id;
    QString label;
    double scale = 1.0;
};

// AudioLanguageDefinition 描述皮肤支持的一种语音语言（jp / en / zh 等）。
struct AudioLanguageDefinition
{
    QString id;
    QString label;
};

// AudioDefinition 是皮肤的可选语音能力声明。
// voiceLanguages 为空时表示该皮肤没有多语言语音，菜单不显示语音子菜单。
struct AudioDefinition
{
    QString defaultVoiceLanguage;
    QList<AudioLanguageDefinition> voiceLanguages;
};

// ExpressionDefinition 声明皮肤支持的一种表达标签（如 objection、polite、tea_break）。
// 模型或 agent 输出表达标签后，再由 ExpressionMapping 把标签映射到具体播放请求。
// allowedStates 限定该表达只能在某些运行时状态下触发。
struct ExpressionDefinition
{
    QString id;
    QString label;
    QString description;
    QStringList allowedStates;
};

// ExpressionMappingEntry 是一条表达 → 请求的映射候选。
// allowedStates 在多状态下做过滤，weight 在 random 选择模式下生效。
struct ExpressionMappingEntry
{
    ActionRequest request;
    QStringList allowedStates;
    int weight = 1;
};

// ExpressionMappingDefinition 把一个 expression tag 映射到一组候选动作。
// selection 控制选取方式（first_available / weighted_random 等），
// fallbackExpressionId 在所有候选都不可用时降级到的兜底表达。
struct ExpressionMappingDefinition
{
    QString selection = "first_available";
    QString fallbackExpressionId = "neutral";
    QList<ExpressionMappingEntry> actions;
};

// HitZoneDefinition 声明一块点击命中区域。
//
// 支持矩形（rect）或多边形（polygon）两种形状；facingRects / facingPolygons
// 让作者为每个朝向各定义一份。zoneId 是自由字符串，框架不假设语义——
// 人形皮肤可用 head/chest，猫娘可用 ears/tail。
struct HitZoneDefinition
{
    QString id;
    QRectF rect;
    QList<QPointF> polygon;
    QHash<QString, QRectF> facingRects;
    QHash<QString, QList<QPointF>> facingPolygons;
};

// ClickBehaviorEntry 把一个 zone 或一个高级 Custom Interaction 绑定到一次点击行为。
// when 默认 "default"，可用于双击的多候选场景（如概率分支）。
struct ClickBehaviorEntry
{
    QString zoneId;
    QString when = "default";
    ActionRequest request;
    QString customInteractionId;
};

// ClickBehaviorDefinition 把单击 / 双击映射成 Zone 或 Custom Interaction。
// singleClick 列表顺序 = 优先级；HitZoneMatcher 按此顺序判命中。
struct ClickBehaviorDefinition
{
    QList<ClickBehaviorEntry> singleClick;
    QList<ClickBehaviorEntry> doubleClick;
};

// SkinManifest 是皮肤包加载后的全量内存模型。
// PetRuntime / 各 Selector / InteractionPipeline / CustomInteraction 全部从这里取数据，
// 不直接读 manifest.json，也不允许在运行时修改它。
struct SkinManifest
{
    // 当前加载的皮肤元信息。Runtime 和菜单只读这些字段，不直接读取 skin.json。
    QString skinId;
    QString skinName;
    QUrl skinRootUrl;
    bool builtin = false;
    QString personaPrompt;

    CanvasDefinition canvas;
    AudioDefinition audio;
    CapabilityDefinition capabilities;
    QList<PetSizeDefinition> sizes;
    QString defaultSizeId;
    QHash<QString, QString> stateToAction;
    QHash<QString, ActionDefinition> actions;
    QHash<QString, ClipDefinition> clips;
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
