#include "pet/PetRuntime.h"

#include "pet/manifest/SkinManifestLoader.h"

#include <QSet>
#include <QSettings>
#include <QVariantMap>

namespace {

bool petSizeScaleForId(const SkinManifest &manifest, const QString &sizeId, double *scale)
{
    const QString normalizedSizeId = sizeId.trimmed();
    if (normalizedSizeId.isEmpty()) {
        return false;
    }

    for (const PetSizeDefinition &size : manifest.sizes) {
        if (size.id == normalizedSizeId && size.scale > 0.0) {
            if (scale != nullptr) {
                *scale = size.scale;
            }
            return true;
        }
    }
    return false;
}

QString defaultFacingForManifest(const SkinManifest &manifest)
{
    if (!manifest.defaultFacing.isEmpty() && manifest.facings.contains(manifest.defaultFacing)) {
        return manifest.defaultFacing;
    }
    if (!manifest.facings.isEmpty()) {
        return manifest.facings.constFirst();
    }
    return QStringLiteral("right");
}

QString defaultMovementDirectionForManifest(const SkinManifest &manifest)
{
    if (manifest.movementDirections.isEmpty()) {
        return {};
    }
    return manifest.movementDirections.constFirst();
}

bool recipeStepsEquivalent(const RecipeStep &left, const RecipeStep &right)
{
    return left.actionId == right.actionId
        && left.phaseId == right.phaseId
        && left.recipeId == right.recipeId
        && left.movementDirection == right.movementDirection
        && left.facing == right.facing
        && left.repeat == right.repeat
        && left.durationMs == right.durationMs
        && left.durationMode == right.durationMode
        && left.runtimeControlled == right.runtimeControlled;
}

bool actionPhaseValid(const SkinManifest &manifest, const QString &actionId, const QString &phaseId)
{
    if (actionId.isEmpty() || !manifest.actions.contains(actionId)) {
        return false;
    }

    const ActionDefinition action = manifest.actions.value(actionId);
    if (action.phases.isEmpty()) {
        return phaseId == QStringLiteral("single");
    }

    return !phaseId.isEmpty() && action.phases.contains(phaseId);
}

bool stateMappingStillValid(
    const SkinManifest &previousManifest,
    const SkinManifest &nextManifest,
    const QString &state
)
{
    if (state.isEmpty()) {
        return false;
    }

    const QString previousAction = previousManifest.stateToAction.value(state);
    const QString nextAction = nextManifest.stateToAction.value(state);
    return !previousAction.isEmpty()
        && previousAction == nextAction
        && nextManifest.actions.contains(nextAction);
}

bool recipePlaybackStillValid(
    const SkinManifest &previousManifest,
    const SkinManifest &nextManifest,
    const QString &recipeId,
    int recipeStepIndex,
    const QString &actionId,
    const QString &phaseId
)
{
    if (recipeId.isEmpty()) {
        return recipeStepIndex == -1;
    }
    if (!previousManifest.recipes.contains(recipeId) || !nextManifest.recipes.contains(recipeId)) {
        return false;
    }

    const RecipeDefinition previousRecipe = previousManifest.recipes.value(recipeId);
    const RecipeDefinition nextRecipe = nextManifest.recipes.value(recipeId);
    if (recipeStepIndex < 0
        || recipeStepIndex >= previousRecipe.steps.size()
        || recipeStepIndex >= nextRecipe.steps.size()) {
        return false;
    }

    const RecipeStep previousStep = previousRecipe.steps.at(recipeStepIndex);
    const RecipeStep nextStep = nextRecipe.steps.at(recipeStepIndex);
    if (!recipeStepsEquivalent(previousStep, nextStep)) {
        return false;
    }
    if (!nextStep.actionId.isEmpty() && nextStep.actionId != actionId) {
        return false;
    }
    if (!nextStep.phaseId.isEmpty() && nextStep.phaseId != phaseId) {
        return false;
    }
    if (!nextStep.recipeId.isEmpty() && !nextManifest.recipes.contains(nextStep.recipeId)) {
        return false;
    }
    if (!nextStep.actionId.isEmpty() && !actionPhaseValid(nextManifest, nextStep.actionId, phaseId)) {
        return false;
    }

    return true;
}

bool playbackStateStillValid(
    const SkinManifest &previousManifest,
    const SkinManifest &nextManifest,
    const QString &state,
    const QString &actionId,
    const QString &phaseId,
    const QString &recipeId,
    int recipeStepIndex
)
{
    return stateMappingStillValid(previousManifest, nextManifest, state)
        && actionPhaseValid(nextManifest, actionId, phaseId)
        && recipePlaybackStillValid(
            previousManifest,
            nextManifest,
            recipeId,
            recipeStepIndex,
            actionId,
            phaseId
        );
}

} // namespace

QVariantList PetRuntime::availableSkins() const
{
    QVariantList skins;
    QSet<QString> seenIds;
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        if (descriptor.id.isEmpty() || seenIds.contains(descriptor.id)) {
            continue;
        }
        seenIds.insert(descriptor.id);

        QVariantMap skin;
        skin.insert(QStringLiteral("id"), descriptor.id);
        skin.insert(QStringLiteral("name"), descriptor.name);
        skin.insert(QStringLiteral("version"), descriptor.version);
        skin.insert(QStringLiteral("thumbnailUrl"), descriptor.thumbnailUrl.toString());
        skins.append(skin);
    }
    return skins;
}

QVariantList PetRuntime::availablePetSizes() const
{
    QVariantList sizes;
    for (const PetSizeDefinition &size : m_manifest.sizes) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), size.id);
        item.insert(QStringLiteral("label"), size.label);
        item.insert(QStringLiteral("scale"), size.scale);
        sizes.append(item);
    }
    return sizes;
}

bool PetRuntime::setActiveSkin(const QString &skinId)
{
    return activateSkin(skinId, true);
}

bool PetRuntime::activateSkin(const QString &skinId, bool persistSelection)
{
    if (m_availableSkinDescriptors.isEmpty()) {
        refreshAvailableSkins();
    }

    QList<SkinDescriptor> descriptors = descriptorsForSkinId(skinId);
    if (descriptors.isEmpty()) {
        refreshAvailableSkins();
        descriptors = descriptorsForSkinId(skinId);
    }

    for (const SkinDescriptor &descriptor : descriptors) {
        if (loadSkinDescriptor(descriptor, SkinReloadMode::PlayStartup)) {
            if (persistSelection) {
                QSettings().setValue(QStringLiteral("skin/activeSkinId"), m_activeSkinId);
            }
            return true;
        }
    }

    return false;
}

bool PetRuntime::reloadActiveSkin()
{
    return reloadActiveSkin(SkinReloadMode::PlayStartup);
}

bool PetRuntime::reloadActiveSkinPreservingPlayback()
{
    return reloadActiveSkin(SkinReloadMode::PreservePlayback);
}

bool PetRuntime::reloadActiveSkin(SkinReloadMode mode)
{
    refreshAvailableSkins();
    for (const SkinDescriptor &descriptor : descriptorsForSkinId(m_activeSkinId)) {
        if (loadSkinDescriptor(descriptor, mode)) {
            return true;
        }
    }
    return false;
}

void PetRuntime::applyManifestState(bool preserveRuntimeState)
{
    const QString previousAudioLanguageId = m_audioController.currentLanguageId();
    const QString previousFacing = m_currentFacing;
    const QString previousMovementDirection = m_currentMovementDirection;
    const QString previousPetSizeId = m_petSizeId;
    const double previousPetScale = m_petScale;

    m_audioController.setAudioDefinition(m_manifest.audio);
    if (preserveRuntimeState && !previousAudioLanguageId.isEmpty()) {
        m_audioController.setLanguage(previousAudioLanguageId);
    }
    emit availableAudioLanguagesChanged();
    if (previousAudioLanguageId != m_audioController.currentLanguageId()) {
        emit currentAudioLanguageChanged();
    }

    QString nextFacing = defaultFacingForManifest(m_manifest);
    if (preserveRuntimeState && m_manifest.facings.contains(previousFacing)) {
        nextFacing = previousFacing;
    }
    m_currentFacing = nextFacing;
    if (previousFacing != m_currentFacing) {
        emit currentFacingChanged();
    }

    QString nextMovementDirection = defaultMovementDirectionForManifest(m_manifest);
    if (preserveRuntimeState && m_manifest.movementDirections.contains(previousMovementDirection)) {
        nextMovementDirection = previousMovementDirection;
    }
    m_currentMovementDirection = nextMovementDirection;
    if (previousMovementDirection != m_currentMovementDirection) {
        emit currentMovementDirectionChanged();
    }

    emit availablePetSizesChanged();
    QString nextSizeId = m_manifest.defaultSizeId.trimmed();
    double nextScale = 0.0;
    bool hasNextSize = petSizeScaleForId(m_manifest, nextSizeId, &nextScale);
    if (preserveRuntimeState) {
        double preservedScale = 0.0;
        if (petSizeScaleForId(m_manifest, previousPetSizeId, &preservedScale)) {
            nextSizeId = previousPetSizeId;
            nextScale = preservedScale;
            hasNextSize = true;
        }
    }
    if (!hasNextSize && !m_manifest.sizes.isEmpty()) {
        nextSizeId = m_manifest.sizes.constFirst().id;
        hasNextSize = petSizeScaleForId(m_manifest, nextSizeId, &nextScale);
    }

    if (hasNextSize) {
        m_petSizeId = nextSizeId;
        m_petScale = nextScale;
        configureMotionController();
        if (previousPetSizeId != m_petSizeId || previousPetScale != m_petScale) {
            emit petScaleChanged();
        }
        return;
    }

    // 极简皮肤可以暂时不声明 sizes。此时必须清掉上一张皮肤的尺寸状态，
    // 否则切换后会继续暴露旧 sizeId / scale。
    m_petSizeId.clear();
    m_petScale = 1.0;
    configureMotionController();
    if (previousPetSizeId != m_petSizeId || previousPetScale != m_petScale) {
        emit petScaleChanged();
    }
}

bool PetRuntime::loadSkinDescriptor(const SkinDescriptor &descriptor, SkinReloadMode mode)
{
    if (descriptor.id.isEmpty()) {
        return false;
    }

    const SkinManifest nextManifest = SkinManifestLoader::loadFromDescriptor(descriptor);
    if (nextManifest.actions.isEmpty()) {
        return false;
    }

    const bool preservePlayback = mode == SkinReloadMode::PreservePlayback;
    const QString preservedState = m_currentState;
    const QString preservedActionId = m_currentActionId;
    const QString preservedRecipeId = m_currentRecipeId;
    const int preservedRecipeStepIndex = m_currentRecipeStepIndex;
    const QString preservedPhaseId = m_currentPhaseId;
    const QString preservedFacing = m_currentFacing;
    const QString preservedMovementDirection = m_currentMovementDirection;
    const QString preservedLoopMode = m_currentLoopMode;
    const bool preservedAutoReturnToIdle = m_currentAutoReturnToIdle;
    const QUrl preservedAnimationUrl = m_currentAnimationUrl;
    const bool preservedPlaybackAtBoundary = m_currentPlaybackAtBoundary;
    const int preservedPlaybackSerial = m_playbackSerial;
    const bool wasPointerInteractionEnabled = pointerInteractionEnabled();
    const bool wasSleeping = sleeping();
    const bool wasSleepTransitioning = sleepTransitioning();
    const bool hadAction = !m_currentActionId.isEmpty();
    const bool hadPhase = !m_currentPhaseId.isEmpty();
    const bool hadMovementDirection = !m_currentMovementDirection.isEmpty();
    const bool loopModeChanged = (m_currentLoopMode != QStringLiteral("loop"));
    const bool autoReturnChanged = m_currentAutoReturnToIdle;
    const bool hadAnimation = !m_currentAnimationUrl.isEmpty();
    const bool hadSound = !m_audioController.currentSoundUrl().isEmpty();
    const bool canPreservePlayback = preservePlayback
        && playbackStateStillValid(
            m_manifest,
            nextManifest,
            preservedState,
            preservedActionId,
            preservedPhaseId,
            preservedRecipeId,
            preservedRecipeStepIndex
        );
    const auto emitDerivedStateChanges = [&]() {
        if (wasPointerInteractionEnabled != pointerInteractionEnabled()) {
            emit pointerInteractionEnabledChanged();
        }
        if (wasSleeping != sleeping()
                || wasSleepTransitioning != sleepTransitioning()) {
            emit sleepStateChanged();
        }
    };

    if (!preservePlayback) {
        hideCurrentProp();
        clearActiveRecipe();
        const bool soundCleared = m_audioController.clearCurrentSound();
        m_currentActionId.clear();
        m_currentPhaseId.clear();
        m_currentMovementDirection.clear();
        m_currentLoopMode = QStringLiteral("loop");
        m_currentAutoReturnToIdle = false;
        m_currentAnimationUrl.clear();
        m_currentPlaybackAtBoundary = false;
        ++m_playbackSerial;

        if (hadAction) {
            emit currentActionChanged();
        }
        if (hadPhase) {
            emit currentPhaseChanged();
        }
        if (hadMovementDirection) {
            emit currentMovementDirectionChanged();
        }
        if (loopModeChanged) {
            emit currentLoopModeChanged();
        }
        if (autoReturnChanged) {
            emit currentAutoReturnToIdleChanged();
        }
        if (wasPointerInteractionEnabled != pointerInteractionEnabled()) {
            emit pointerInteractionEnabledChanged();
        }
        if (hadAnimation) {
            emit currentAnimationUrlChanged();
        }
        if (hadSound && soundCleared) {
            emit currentSoundUrlChanged();
            emit soundPlaybackSerialChanged();
        }
        if (wasSleeping != sleeping()
                || wasSleepTransitioning != sleepTransitioning()) {
            emit sleepStateChanged();
        }
        emit playbackSerialChanged();
    }

    m_manifest = nextManifest;
    m_activeSkinId = descriptor.id;

    applyManifestState(preservePlayback);
    if (preservePlayback) {
        if (!canPreservePlayback) {
            hideCurrentProp();
            const bool soundCleared = m_audioController.clearCurrentSound();
            setState(QStringLiteral("idle"));
            if (soundCleared) {
                emit currentSoundUrlChanged();
                emit soundPlaybackSerialChanged();
            }
            emitDerivedStateChanges();
            emit activeSkinChanged();
            emit skinManifestReloaded();
            return true;
        }

        m_currentState = preservedState;
        m_currentActionId = preservedActionId;
        m_currentRecipeId = preservedRecipeId;
        m_currentRecipeStepIndex = preservedRecipeStepIndex;
        m_currentRecipeStepRuntimeControlled = false;
        if (!m_currentRecipeId.isEmpty() && m_manifest.recipes.contains(m_currentRecipeId)) {
            const RecipeDefinition currentRecipe = m_manifest.recipes.value(m_currentRecipeId);
            if (m_currentRecipeStepIndex >= 0 && m_currentRecipeStepIndex < currentRecipe.steps.size()) {
                const RecipeStep currentStep = currentRecipe.steps.at(m_currentRecipeStepIndex);
                m_currentRecipeStepRuntimeControlled = currentStep.runtimeControlled
                    && currentStep.phaseId == QStringLiteral("loop");
            }
        }
        m_currentPhaseId = preservedPhaseId;
        if (m_manifest.facings.contains(preservedFacing)) {
            m_currentFacing = preservedFacing;
        }
        if (m_manifest.movementDirections.contains(preservedMovementDirection)) {
            m_currentMovementDirection = preservedMovementDirection;
        }
        const ActionDefinition currentAction = m_manifest.actions.value(m_currentActionId);
        AnimationVariant currentVariant;
        QString currentLoopMode = preservedLoopMode;
        if (!currentAction.phases.isEmpty() && currentAction.phases.contains(m_currentPhaseId)) {
            const PhaseDefinition currentPhase = currentAction.phases.value(m_currentPhaseId);
            currentLoopMode = currentPhase.loopMode.isEmpty() ? QStringLiteral("loop") : currentPhase.loopMode;
            currentVariant = variantForFacing(currentPhase.variants, m_currentFacing);
        } else {
            currentLoopMode = currentAction.loopMode.isEmpty() ? QStringLiteral("loop") : currentAction.loopMode;
            currentVariant = variantForAction(currentAction);
        }

        if (m_currentActionId != m_manifest.fallbackAction && !animationUrlPlayable(currentVariant.url)) {
            hideCurrentProp();
            clearActiveRecipe();
            const bool soundCleared = m_audioController.clearCurrentSound();
            setState(QStringLiteral("idle"));
            if (soundCleared) {
                emit currentSoundUrlChanged();
                emit soundPlaybackSerialChanged();
            }
            emitDerivedStateChanges();
            emit activeSkinChanged();
            emit skinManifestReloaded();
            return true;
        }

        const bool currentAutoReturnToIdle = (currentLoopMode == QStringLiteral("onceThenIdle"));
        const bool loopModeChangedDuringPreserve = (preservedLoopMode != currentLoopMode);
        const bool autoReturnChangedDuringPreserve = (preservedAutoReturnToIdle != currentAutoReturnToIdle);
        const bool playbackMetadataChanged = (preservedAnimationUrl != currentVariant.url);

        m_currentLoopMode = currentLoopMode;
        m_currentAutoReturnToIdle = currentAutoReturnToIdle;
        m_currentAnimationUrl = currentVariant.url;
        m_currentPlaybackAtBoundary = (playbackMetadataChanged
                || loopModeChangedDuringPreserve
                || autoReturnChangedDuringPreserve)
            ? false
            : preservedPlaybackAtBoundary;
        m_playbackSerial = preservedPlaybackSerial;
        if (playbackMetadataChanged) {
            ++m_playbackSerial;
        }

        emitDerivedStateChanges();
        if (loopModeChangedDuringPreserve) {
            emit currentLoopModeChanged();
        }
        if (autoReturnChangedDuringPreserve) {
            emit currentAutoReturnToIdleChanged();
        }
        if (playbackMetadataChanged) {
            emit currentAnimationUrlChanged();
            emit playbackSerialChanged();
        }

        emit activeSkinChanged();
        emit skinManifestReloaded();
        return true;
    }

    setState(QStringLiteral("idle"));
    emit activeSkinChanged();
    emit skinManifestReloaded();
    startStartupSequence();
    return true;
}

QList<SkinDescriptor> PetRuntime::descriptorsForSkinId(const QString &skinId) const
{
    QList<SkinDescriptor> descriptors;
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        if (descriptor.id == skinId) {
            descriptors.append(descriptor);
        }
    }
    return descriptors;
}

void PetRuntime::refreshAvailableSkins()
{
    m_availableSkinDescriptors = SkinManifestLoader::discoverAll();
    emit availableSkinsChanged();
}
