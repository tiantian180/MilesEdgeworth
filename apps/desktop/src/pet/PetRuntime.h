#pragma once

#include "pet/manifest/SkinManifest.h"
#include "pet/effects/AudioController.h"
#include "pet/effects/PropController.h"
#include "pet/events/PetEvent.h"
#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <QHash>
#include <QJSEngine>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QRectF>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// PetRuntime 是 v2 桌宠动画系统的执行入口。
//
// 它不再理解“单击”“双击”“红茶”这类事件语义；这些语义先进入
// PetEventBridge / InteractionPipeline，再转换成 ActionRequest。Runtime 只负责：
// 1. 从 skin manifest 解析 action / recipe / pool。
// 2. 执行 ActionRequest 并更新当前动画、音效、Prop 和状态信号。
// 3. 提供 RuntimeSnapshot 给交互层读取当前状态。
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
    Q_PROPERTY(QString currentAudioLanguageId READ currentAudioLanguageId NOTIFY currentAudioLanguageChanged)
    Q_PROPERTY(QVariantList availableAudioLanguages READ availableAudioLanguages CONSTANT)
    Q_PROPERTY(bool autoMovementEnabled READ autoMovementEnabled NOTIFY autoMovementEnabledChanged)
    Q_PROPERTY(QString petSizeId READ petSizeId NOTIFY petScaleChanged)
    Q_PROPERTY(QVariantList availablePetSizes READ availablePetSizes CONSTANT)
    Q_PROPERTY(double petScale READ petScale NOTIFY petScaleChanged)
    Q_PROPERTY(double petWindowSize READ petWindowSize NOTIFY petScaleChanged)
    Q_PROPERTY(double petImageSize READ petImageSize NOTIFY petScaleChanged)
    Q_PROPERTY(bool pointerInteractionEnabled READ pointerInteractionEnabled NOTIFY pointerInteractionEnabledChanged)
    Q_PROPERTY(bool sleeping READ sleeping NOTIFY sleepStateChanged)
    Q_PROPERTY(bool sleepTransitioning READ sleepTransitioning NOTIFY sleepStateChanged)
    Q_PROPERTY(QUrl currentAnimationUrl READ currentAnimationUrl NOTIFY currentAnimationUrlChanged)
    Q_PROPERTY(QUrl currentSoundUrl READ currentSoundUrl NOTIFY currentSoundUrlChanged)
    Q_PROPERTY(bool currentPropVisible READ currentPropVisible NOTIFY currentPropChanged)
    Q_PROPERTY(QString currentPropId READ currentPropId NOTIFY currentPropChanged)
    Q_PROPERTY(QUrl currentPropImageUrl READ currentPropImageUrl NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropStartX READ currentPropStartX NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropStartY READ currentPropStartY NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropEndX READ currentPropEndX NOTIFY currentPropChanged)
    Q_PROPERTY(double currentPropEndY READ currentPropEndY NOTIFY currentPropChanged)
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

    QString currentState() const { return m_currentState; }
    QString currentActionId() const { return m_currentActionId; }
    QString currentRecipeId() const { return m_currentRecipeId; }
    QString currentPhaseId() const { return m_currentPhaseId; }
    QString currentFacing() const { return m_currentFacing; }
    QString currentMovementDirection() const { return m_currentMovementDirection; }
    QString currentLoopMode() const { return m_currentLoopMode; }
    bool currentAutoReturnToIdle() const { return m_currentAutoReturnToIdle; }
    bool audioMuted() const { return m_audioController.muted(); }
    QString currentAudioLanguageId() const { return m_audioController.currentLanguageId(); }
    QVariantList availableAudioLanguages() const { return m_audioController.availableLanguages(); }
    bool autoMovementEnabled() const { return m_autoMovementEnabled; }
    QString petSizeId() const { return m_petSizeId; }
    QVariantList availablePetSizes() const;
    double petScale() const { return m_petScale; }
    double petWindowSize() const { return m_manifest.canvas.windowSize * m_petScale; }
    double petImageSize() const { return m_manifest.canvas.imageSize * m_petScale; }
    bool pointerInteractionEnabled() const { return acceptsPointerInteraction(); }
    bool restCapabilityEnabled() const { return m_manifest.capabilities.rest.enabled(); }
    bool currentActionAcceptsIdleLoopFinished() const;
    bool sleeping() const;
    bool sleepTransitioning() const;
    QUrl currentAnimationUrl() const { return m_currentAnimationUrl; }
    QUrl currentSoundUrl() const { return m_audioController.currentSoundUrl(); }
    bool currentPropVisible() const { return m_propController.current().visible; }
    QString currentPropId() const { return m_propController.current().id; }
    QUrl currentPropImageUrl() const { return m_propController.current().imageUrl; }
    double currentPropStartX() const { return m_propController.current().startOffset.x(); }
    double currentPropStartY() const { return m_propController.current().startOffset.y(); }
    double currentPropEndX() const { return m_propController.current().endOffset.x(); }
    double currentPropEndY() const { return m_propController.current().endOffset.y(); }
    double currentPropWidth() const { return m_propController.current().width; }
    double currentPropHeight() const { return m_propController.current().height; }
    double currentPropVisualWidth() const { return m_propController.current().visualWidth; }
    double currentPropVisualHeight() const { return m_propController.current().visualHeight; }
    int currentPropDurationMs() const { return m_propController.current().durationMs; }
    int currentPropPlaybackSerial() const { return m_propController.playbackSerial(); }
    int playbackSerial() const { return m_playbackSerial; }
    int soundPlaybackSerial() const { return m_audioController.playbackSerial(); }
    const SkinManifest &manifest() const { return m_manifest; }
    RuntimeSnapshot snapshot() const;

    Q_INVOKABLE void setState(const QString &state);
    Q_INVOKABLE void setFacing(const QString &facing);
    Q_INVOKABLE void toggleFacing();
    Q_INVOKABLE void playAction(const QString &actionId);
    Q_INVOKABLE void playLocomotion(const QString &actionId, const QString &movementDirection);
    Q_INVOKABLE void playRecipe(const QString &recipeId);
    Q_INVOKABLE void playActionFromPool(const QString &poolId);
    Q_INVOKABLE QVariantMap consumeFrameMovementDelta() const;
    Q_INVOKABLE void toggleAudioMuted();
    Q_INVOKABLE void setAudioLanguage(const QString &languageId);
    Q_INVOKABLE void toggleAutoMovementEnabled();
    Q_INVOKABLE void setPetSize(const QString &sizeId);
    Q_INVOKABLE void startStartupSequence();
    Q_INVOKABLE void returnToIdle();
    Q_INVOKABLE void requestExpression(const QString &state, const QString &expression);
    Q_INVOKABLE void handleAnimationFinished();
    void submitActionRequest(const ActionRequest &request);
    void submitExpressionRequest(const QString &state, const QString &expression, double randomValue);

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
    void currentAudioLanguageChanged();
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
    QString actionForState(const QString &state) const;
    QUrl variantForFacing(const QHash<QString, QUrl> &variants, const QString &facing) const;
    QUrl variantForAction(const ActionDefinition &action) const;
    bool acceptsPointerInteraction() const;
    void playSoundForRecipe(const RecipeDefinition &recipe);
    void hideCurrentProp();
    void clearActiveRecipe();
    void playActionInternal(const QString &actionId, bool resetRecipe);
    void playNextRecipeStep();
    void playRecipeStep(const RecipeStep &step);
    QString resolveRecipeMovementDirection(const QString &movementDirection) const;
    QString resolveRecipeFacing(const QString &facing) const;
    double movementScaleFactor() const;
    bool submitRuntimeEvent(const PetEvent &event);
    void applyFacingAfterCurrentAction(const ActionDefinition &action);
    void updateFacingFromMovementDirection(const QString &movementDirection);
    void playPhase(const QString &actionId, const QString &phaseId);
    void setCurrentAction(const QString &actionId, const ActionDefinition &action);
    void setCurrentPhase(const QString &actionId, const QString &phaseId, const PhaseDefinition &phase);

    SkinManifest m_manifest;
    QString m_currentState = "idle";
    QString m_currentActionId;
    QString m_currentRecipeId;
    int m_currentRecipeStepIndex = -1;
    QString m_currentPhaseId;
    QString m_currentFacing = "right";
    QString m_currentMovementDirection;
    QString m_currentLoopMode = "loop";
    bool m_currentAutoReturnToIdle = false;
    AudioController m_audioController;
    bool m_autoMovementEnabled = true;
    QString m_petSizeId;
    double m_petScale = 2.0;
    QUrl m_currentAnimationUrl;
    PropController m_propController;
    int m_playbackSerial = 0;
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
