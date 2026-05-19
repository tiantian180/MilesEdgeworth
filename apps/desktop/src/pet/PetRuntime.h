#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJSEngine>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QQmlEngine>
#include <QRectF>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// PetRuntime 是 v2 桌宠动画系统的最小入口。
//
// 当前阶段它只做三件事：
// 1. 从内置 skin manifest 里读出 state -> action -> animation 的映射。
// 2. 暴露 currentAnimationUrl 给 QML 的 AnimatedImage 使用。
// 3. 提供几个开发测试入口，方便右键菜单验证状态切换。
//
// Phase 0.6 开始引入“朝向”和“动作播放模式”，但仍然不做完整编排器。
// 后续的 enter/loop/exit、移动驱动动画、点击交互优先级，会继续在
// Pet Runtime / Animation Orchestrator 里分层扩展。
class PetRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentState READ currentState NOTIFY currentStateChanged)
    Q_PROPERTY(QString currentActionId READ currentActionId NOTIFY currentActionChanged)
    Q_PROPERTY(QString currentRecipeId READ currentRecipeId NOTIFY currentRecipeChanged)
    Q_PROPERTY(QString currentPhaseId READ currentPhaseId NOTIFY currentPhaseChanged)
    Q_PROPERTY(QString currentFacing READ currentFacing NOTIFY currentFacingChanged)
    Q_PROPERTY(QString currentMovementDirection READ currentMovementDirection NOTIFY currentMovementDirectionChanged)
    Q_PROPERTY(QString currentLoopMode READ currentLoopMode NOTIFY currentLoopModeChanged)
    Q_PROPERTY(bool currentAutoReturnToIdle READ currentAutoReturnToIdle NOTIFY currentAutoReturnToIdleChanged)
    Q_PROPERTY(bool audioMuted READ audioMuted NOTIFY audioMutedChanged)
    Q_PROPERTY(QString voiceLanguage READ voiceLanguage NOTIFY voiceLanguageChanged)
    Q_PROPERTY(bool autoMovementEnabled READ autoMovementEnabled NOTIFY autoMovementEnabledChanged)
    Q_PROPERTY(QString petSizeId READ petSizeId NOTIFY petScaleChanged)
    Q_PROPERTY(double petScale READ petScale NOTIFY petScaleChanged)
    Q_PROPERTY(double petWindowSize READ petWindowSize NOTIFY petScaleChanged)
    Q_PROPERTY(double petImageSize READ petImageSize NOTIFY petScaleChanged)
    Q_PROPERTY(bool pointerInteractionEnabled READ pointerInteractionEnabled NOTIFY pointerInteractionEnabledChanged)
    Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepStateChanged)
    Q_PROPERTY(bool sleepTransitioning READ sleepTransitioning NOTIFY sleepStateChanged)
    Q_PROPERTY(bool teaEnabled READ teaEnabled NOTIFY sleepStateChanged)
    Q_PROPERTY(QUrl currentAnimationUrl READ currentAnimationUrl NOTIFY currentAnimationUrlChanged)
    Q_PROPERTY(QUrl currentSoundUrl READ currentSoundUrl NOTIFY currentSoundUrlChanged)
    Q_PROPERTY(bool currentPropVisible READ currentPropVisible NOTIFY currentPropChanged)
    Q_PROPERTY(QString currentPropId READ currentPropId NOTIFY currentPropChanged)
    Q_PROPERTY(QUrl currentPropImageUrl READ currentPropImageUrl NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropStartOffsetX READ currentPropStartOffsetX NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropStartOffsetY READ currentPropStartOffsetY NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropEndOffsetX READ currentPropEndOffsetX NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropEndOffsetY READ currentPropEndOffsetY NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropWidth READ currentPropWidth NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropHeight READ currentPropHeight NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropVisualWidth READ currentPropVisualWidth NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropVisualHeight READ currentPropVisualHeight NOTIFY currentPropChanged)
    Q_PROPERTY(int currentPropDurationMs READ currentPropDurationMs NOTIFY currentPropChanged)
    Q_PROPERTY(int currentPropPlaybackSerial READ currentPropPlaybackSerial NOTIFY currentPropPlaybackSerialChanged)
    Q_PROPERTY(int playbackSerial READ playbackSerial NOTIFY playbackSerialChanged)
    Q_PROPERTY(int soundPlaybackSerial READ soundPlaybackSerial NOTIFY soundPlaybackSerialChanged)

public:
    explicit PetRuntime(QObject *parent = nullptr);

    QString currentState() const;
    QString currentActionId() const;
    QString currentRecipeId() const;
    QString currentPhaseId() const;
    QString currentFacing() const;
    QString currentMovementDirection() const;
    QString currentLoopMode() const;
    bool currentAutoReturnToIdle() const;
    bool audioMuted() const;
    QString voiceLanguage() const;
    bool autoMovementEnabled() const;
    QString petSizeId() const;
    double petScale() const;
    double petWindowSize() const;
    double petImageSize() const;
    bool pointerInteractionEnabled() const;
    bool sleeping() const;
    bool sleepTransitioning() const;
    bool teaEnabled() const;
    QUrl currentAnimationUrl() const;
    QUrl currentSoundUrl() const;
    bool currentPropVisible() const;
    QString currentPropId() const;
    QUrl currentPropImageUrl() const;
    double currentPropStartOffsetX() const;
    double currentPropStartOffsetY() const;
    double currentPropEndOffsetX() const;
    double currentPropEndOffsetY() const;
    double currentPropWidth() const;
    double currentPropHeight() const;
    double currentPropVisualWidth() const;
    double currentPropVisualHeight() const;
    int currentPropDurationMs() const;
    int currentPropPlaybackSerial() const;
    int playbackSerial() const;
    int soundPlaybackSerial() const;

    Q_INVOKABLE void setState(const QString &state);
    Q_INVOKABLE void setFacing(const QString &facing);
    Q_INVOKABLE void toggleFacing();
    Q_INVOKABLE void playAction(const QString &actionId);
    Q_INVOKABLE void playLocomotion(const QString &actionId, const QString &movementDirection);
    Q_INVOKABLE void playRecipe(const QString &recipeId);
    Q_INVOKABLE void playActionFromPool(const QString &poolId);
    Q_INVOKABLE QVariantMap consumeFrameMovementDelta() const;
    Q_INVOKABLE void handlePrimaryClick(double x, double y, double width, double height);
    Q_INVOKABLE void handleDoubleClick();
    Q_INVOKABLE void handleDragStarted(double globalX);
    Q_INVOKABLE void handleDragMoved(double globalX);
    Q_INVOKABLE void handleDragEnded();
    Q_INVOKABLE void handleHoldAnimationReachedEnd();
    Q_INVOKABLE void handlePropClicked();
    Q_INVOKABLE void handlePropExpired();
    Q_INVOKABLE void toggleAudioMuted();
    Q_INVOKABLE void setVoiceLanguage(const QString &voiceLanguage);
    Q_INVOKABLE void toggleAutoMovementEnabled();
    Q_INVOKABLE void setPetSize(const QString &sizeId);
    Q_INVOKABLE void requestTea();
    Q_INVOKABLE void toggleSleep();
    Q_INVOKABLE void triggerIdle();
    Q_INVOKABLE void handleIdleLoopFinished();
    Q_INVOKABLE void startStartupSequence();
    Q_INVOKABLE void returnToIdle();
    Q_INVOKABLE void testThinking();
    Q_INVOKABLE void testSpeaking();
    Q_INVOKABLE void testObjecting();
    Q_INVOKABLE void testTurn();
    Q_INVOKABLE void testWalk();
    Q_INVOKABLE void testRun();
    Q_INVOKABLE void testBow();
    Q_INVOKABLE void testTea();
    Q_INVOKABLE void testSleep();
    Q_INVOKABLE void testProsecutorBadge();
    Q_INVOKABLE void handleAnimationFinished();
    void handleIdleLoopFinishedForTest(double randomValue);

signals:
    void currentStateChanged();
    void currentActionChanged();
    void currentRecipeChanged();
    void currentPhaseChanged();
    void currentFacingChanged();
    void currentMovementDirectionChanged();
    void currentLoopModeChanged();
    void currentAutoReturnToIdleChanged();
    void audioMutedChanged();
    void voiceLanguageChanged();
    void autoMovementEnabledChanged();
    void petScaleChanged();
    void pointerInteractionEnabledChanged();
    void sleepStateChanged();
    void currentAnimationUrlChanged();
    void currentSoundUrlChanged();
    void currentPropChanged();
    void currentPropPlaybackSerialChanged();
    void playbackSerialChanged();
    void soundPlaybackSerialChanged();

private:
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

    struct BehaviorTriggerEntry
    {
        QString type;
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

    struct HitZoneDefinition
    {
        QString id;
        QRectF rect;
        QList<QPointF> polygon;
        QHash<QString, QRectF> facingRects;
        QHash<QString, QList<QPointF>> facingPolygons;
    };

    void loadManifest();
    void loadFallbackManifest();
    QString actionForState(const QString &state) const;
    QUrl variantForFacing(const QHash<QString, QUrl> &variants, const QString &facing) const;
    QUrl variantForAction(const ActionDefinition &action) const;
    QRectF rectForHitZone(const HitZoneDefinition &zone) const;
    QList<QPointF> polygonForHitZone(const HitZoneDefinition &zone) const;
    bool hitZoneContainsPoint(const HitZoneDefinition &zone, const QPointF &point) const;
    QString clickPoolForPoint(double x, double y, double width, double height) const;
    QUrl soundUrlForRecipe(const RecipeDefinition &recipe) const;
    bool acceptsPointerInteraction() const;
    void playSoundForRecipe(const RecipeDefinition &recipe);
    void schedulePropForRecipe(const RecipeDefinition &recipe);
    void spawnPropForRecipe(const QString &propId, const QString &facing);
    void hideCurrentProp();
    void clearActiveRecipe();
    void playActionInternal(const QString &actionId, bool resetRecipe);
    void playNextRecipeStep();
    void playRecipeStep(const RecipeStep &step);
    QString actionPoolIdForContext(const QString &poolId) const;
    ActionPoolEntry selectActionPoolEntry(const ActionPoolDefinition &pool) const;
    BehaviorTriggerEntry selectBehaviorTriggerEntry(const BehaviorTriggerDefinition &trigger, double randomValue) const;
    bool behaviorTriggerMatchesCurrentContext(const BehaviorTriggerDefinition &trigger) const;
    void handleBehaviorTriggerWithRoll(const QString &triggerId, double randomValue);
    QString followUpPoolForCompletedAction(const ActionDefinition &action) const;
    QString resolveRecipeMovementDirection(const QString &movementDirection) const;
    QString resolveRecipeFacing(const QString &facing) const;
    double movementScaleFactor() const;
    double scaledPropLength(double length) const;
    QPointF propPointForFacing(const QHash<QString, QPointF> &points, const QString &facing) const;
    QPointF scaledPropPoint(const QPointF &point) const;
    QPointF propTravelDelta(const PropDefinition &prop, const QString &facing) const;
    void handleIdleLoopFinishedWithRoll(double randomValue);
    void applyFacingAfterCurrentAction(const ActionDefinition &action);
    void updateFacingFromMovementDirection(const QString &movementDirection);
    void playPhase(const QString &actionId, const QString &phaseId);
    void setCurrentAction(const QString &actionId, const ActionDefinition &action);
    void setCurrentPhase(const QString &actionId, const QString &phaseId, const PhaseDefinition &phase);

    QHash<QString, QString> m_stateToAction;
    QHash<QString, ActionDefinition> m_actions;
    QHash<QString, RecipeDefinition> m_recipes;
    QHash<QString, ActionPoolDefinition> m_actionPools;
    QHash<QString, BehaviorTriggerDefinition> m_behaviorTriggers;
    QHash<QString, PropDefinition> m_props;
    QHash<QString, HitZoneDefinition> m_hitZones;
    QStringList m_singleClickPools;
    QString m_fallbackAction = "idle_stand";
    QStringList m_facings = {"right", "left"};
    QStringList m_movementDirections;
    QString m_defaultFacing = "right";
    QString m_currentState = "idle";
    QString m_currentActionId;
    QString m_currentRecipeId;
    int m_currentRecipeStepIndex = -1;
    QString m_currentPhaseId;
    QString m_currentFacing = "right";
    QString m_currentMovementDirection = "east";
    QString m_currentLoopMode = "loop";
    bool m_currentAutoReturnToIdle = false;
    bool m_audioMuted = false;
    QString m_voiceLanguage = "jp";
    bool m_autoMovementEnabled = true;
    QString m_petSizeId = "medium";
    double m_petScale = 2.0;
    QUrl m_currentAnimationUrl;
    QUrl m_currentSoundUrl;
    bool m_currentPropVisible = false;
    QString m_currentPropId;
    QUrl m_currentPropImageUrl;
    QPointF m_currentPropStartOffset;
    QPointF m_currentPropEndOffset;
    double m_currentPropWidth = 0;
    double m_currentPropHeight = 0;
    double m_currentPropVisualWidth = 0;
    double m_currentPropVisualHeight = 0;
    int m_currentPropDurationMs = 0;
    QString m_currentPropClickedRecipeId;
    QString m_currentPropExpiredRecipeId;
    int m_currentPropPlaybackSerial = 0;
    int m_propRequestSerial = 0;
    int m_playbackSerial = 0;
    int m_soundPlaybackSerial = 0;
    QElapsedTimer m_dragShakeClock;
    double m_dragShakeX = 0;
    int m_dragShakeDirection = 1;
    int m_dragShakeTurns = 0;
    bool m_dragShakeTracking = false;
    bool m_dragHoldAnimationCompleted = false;
};

// 与 DesktopShellControllerForeign 一样，这个 wrapper 让 QML 看到一个名为
// PetRuntime 的单例，但真实对象仍由 main.cpp 创建和持有。
struct PetRuntimeForeign
{
    Q_GADGET
    QML_FOREIGN(PetRuntime)
    QML_NAMED_ELEMENT(PetRuntime)
    QML_SINGLETON

public:
    inline static PetRuntime *s_instance = nullptr;

    static PetRuntime *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());

        // QML 只借用桌宠运行时；C++ 负责生命周期，避免引擎退出时误删栈对象。
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
