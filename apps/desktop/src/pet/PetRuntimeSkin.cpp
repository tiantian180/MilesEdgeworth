#include "pet/PetRuntime.h"

#include "pet/manifest/SkinManifestLoader.h"

#include <QSettings>
#include <QVariantMap>

QVariantList PetRuntime::availableSkins() const
{
    QVariantList skins;
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        QVariantMap skin;
        skin.insert(QStringLiteral("id"), descriptor.id);
        skin.insert(QStringLiteral("name"), descriptor.name);
        skin.insert(QStringLiteral("version"), descriptor.version);
        skin.insert(QStringLiteral("builtin"), descriptor.builtin);
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

    SkinDescriptor descriptor = descriptorForSkinId(skinId);
    if (descriptor.id.isEmpty()) {
        refreshAvailableSkins();
        descriptor = descriptorForSkinId(skinId);
    }

    if (!loadSkinDescriptor(descriptor)) {
        return false;
    }

    if (persistSelection) {
        QSettings().setValue(QStringLiteral("skin/activeSkinId"), m_activeSkinId);
    }
    return true;
}

bool PetRuntime::reloadActiveSkin()
{
    refreshAvailableSkins();
    return loadSkinDescriptor(descriptorForSkinId(m_activeSkinId));
}

void PetRuntime::applyManifestState()
{
    const QString previousAudioLanguageId = m_audioController.currentLanguageId();
    const QString previousFacing = m_currentFacing;
    const QString previousMovementDirection = m_currentMovementDirection;
    const QString previousPetSizeId = m_petSizeId;
    const double previousPetScale = m_petScale;

    m_audioController.setAudioDefinition(m_manifest.audio);
    emit availableAudioLanguagesChanged();
    if (previousAudioLanguageId != m_audioController.currentLanguageId()) {
        emit currentAudioLanguageChanged();
    }

    m_currentFacing = m_manifest.defaultFacing.isEmpty()
        ? QStringLiteral("right")
        : m_manifest.defaultFacing;
    if (previousFacing != m_currentFacing) {
        emit currentFacingChanged();
    }

    m_currentMovementDirection = m_manifest.movementDirections.isEmpty()
        ? QString()
        : m_manifest.movementDirections.constFirst();
    if (previousMovementDirection != m_currentMovementDirection) {
        emit currentMovementDirectionChanged();
    }

    emit availablePetSizesChanged();
    QString nextSizeId = m_manifest.defaultSizeId.trimmed();
    bool hasNextSize = false;
    for (const PetSizeDefinition &size : m_manifest.sizes) {
        if (nextSizeId == size.id) {
            hasNextSize = true;
            break;
        }
    }
    if (!hasNextSize && !m_manifest.sizes.isEmpty()) {
        nextSizeId = m_manifest.sizes.constFirst().id;
        hasNextSize = !nextSizeId.isEmpty();
    }

    if (hasNextSize) {
        setPetSize(nextSizeId);
        return;
    }

    // 极简皮肤可以暂时不声明 sizes。此时必须清掉上一张皮肤的尺寸状态，
    // 否则切换后会继续暴露旧 sizeId / scale。
    m_petSizeId.clear();
    m_petScale = 1.0;
    if (previousPetSizeId != m_petSizeId || previousPetScale != m_petScale) {
        emit petScaleChanged();
    }
}

bool PetRuntime::loadSkinDescriptor(const SkinDescriptor &descriptor)
{
    if (descriptor.id.isEmpty()) {
        return false;
    }

    const SkinManifest nextManifest = SkinManifestLoader::loadFromDescriptor(descriptor);
    if (nextManifest.actions.isEmpty()) {
        return false;
    }

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

    hideCurrentProp();
    clearActiveRecipe();
    const bool soundCleared = m_audioController.clearCurrentSound();
    m_currentActionId.clear();
    m_currentPhaseId.clear();
    m_currentMovementDirection.clear();
    m_currentLoopMode = QStringLiteral("loop");
    m_currentAutoReturnToIdle = false;
    m_currentAnimationUrl.clear();
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

    m_manifest = nextManifest;
    m_activeSkinId = descriptor.id;

    applyManifestState();
    setState(QStringLiteral("idle"));
    emit activeSkinChanged();
    emit skinManifestReloaded();
    startStartupSequence();
    return true;
}

SkinDescriptor PetRuntime::descriptorForSkinId(const QString &skinId) const
{
    for (const SkinDescriptor &descriptor : m_availableSkinDescriptors) {
        if (descriptor.id == skinId) {
            return descriptor;
        }
    }
    return {};
}

void PetRuntime::refreshAvailableSkins()
{
    m_availableSkinDescriptors = SkinManifestLoader::discoverAll();
    emit availableSkinsChanged();
}
