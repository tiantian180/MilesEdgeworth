#include "pet/effects/AudioController.h"

#include <QVariantMap>

void AudioController::setAudioDefinition(const AudioDefinition &audio)
{
    m_audio = audio;

    const QString defaultLanguageId = m_audio.defaultVoiceLanguage.trimmed();
    if (!defaultLanguageId.isEmpty() && (m_audio.voiceLanguages.isEmpty() || hasLanguage(defaultLanguageId))) {
        // defaultVoiceLanguage 是声音选择的默认值；voiceLanguages 只是菜单可选项。
        // 皮肤可以只声明默认语言而不显示语言菜单。
        m_currentLanguageId = defaultLanguageId;
        return;
    }

    if (!m_audio.voiceLanguages.isEmpty()) {
        m_currentLanguageId = m_audio.voiceLanguages.constFirst().id;
        return;
    }

    m_currentLanguageId.clear();
}

bool AudioController::toggleMuted()
{
    m_muted = !m_muted;
    return m_muted;
}

QVariantList AudioController::availableLanguages() const
{
    QVariantList languages;
    for (const AudioLanguageDefinition &language : m_audio.voiceLanguages) {
        if (language.id.isEmpty()) {
            continue;
        }

        QVariantMap item;
        item.insert(QStringLiteral("id"), language.id);
        item.insert(QStringLiteral("label"), language.label.isEmpty() ? language.id : language.label);
        languages.append(item);
    }
    return languages;
}

bool AudioController::setLanguage(const QString &languageId)
{
    const QString normalizedLanguageId = languageId.trimmed();
    if (normalizedLanguageId.isEmpty() || normalizedLanguageId == m_currentLanguageId || !hasLanguage(normalizedLanguageId)) {
        return false;
    }

    m_currentLanguageId = normalizedLanguageId;
    return true;
}

bool AudioController::clearCurrentSound()
{
    if (m_currentSoundUrl.isEmpty()) {
        return false;
    }

    m_currentSoundUrl = QUrl();
    ++m_playbackSerial;
    return true;
}

QUrl AudioController::soundUrlForRecipe(const RecipeDefinition &recipe) const
{
    if (!m_currentLanguageId.isEmpty() && recipe.soundUrls.contains(m_currentLanguageId)) {
        return recipe.soundUrls.value(m_currentLanguageId);
    }

    if (!recipe.soundUrls.isEmpty()) {
        return recipe.soundUrls.constBegin().value();
    }

    return recipe.soundUrl;
}

bool AudioController::playSoundForRecipe(const RecipeDefinition &recipe, bool *soundChanged)
{
    return playSound(soundUrlForRecipe(recipe), soundChanged);
}

bool AudioController::playSound(const QUrl &url, bool *soundChanged)
{
    if (soundChanged != nullptr) {
        *soundChanged = false;
    }

    if (m_muted || url.isEmpty()) {
        return false;
    }

    const bool changed = (m_currentSoundUrl != url);
    m_currentSoundUrl = url;
    ++m_playbackSerial;

    if (soundChanged != nullptr) {
        *soundChanged = changed;
    }
    return true;
}

bool AudioController::hasLanguage(const QString &languageId) const
{
    if (languageId.trimmed().isEmpty()) {
        return false;
    }

    for (const AudioLanguageDefinition &language : m_audio.voiceLanguages) {
        if (language.id == languageId) {
            return true;
        }
    }
    return false;
}
