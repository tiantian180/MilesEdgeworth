#pragma once

#include "pet/manifest/SkinManifest.h"

#include <QUrl>
#include <QVariantList>

// AudioController 管理声音播放请求和可选语音语言。
//
// 它只处理“当前语言应该选哪条声音资源”“静音时是否允许新声音请求”
// 以及播放序号；菜单、交互事件和动画调度仍留在各自层里。
class AudioController
{
public:
    void setAudioDefinition(const AudioDefinition &audio);

    bool muted() const { return m_muted; }
    bool toggleMuted();

    QString currentLanguageId() const { return m_currentLanguageId; }
    QVariantList availableLanguages() const;
    bool setLanguage(const QString &languageId);

    QUrl currentSoundUrl() const { return m_currentSoundUrl; }
    int playbackSerial() const { return m_playbackSerial; }

    QUrl soundUrlForRecipe(const RecipeDefinition &recipe) const;
    bool playSoundForRecipe(const RecipeDefinition &recipe, bool *soundChanged = nullptr);
    bool playSound(const QUrl &url, bool *soundChanged = nullptr);

private:
    bool hasLanguage(const QString &languageId) const;

    AudioDefinition m_audio;
    QString m_currentLanguageId;
    bool m_muted = false;
    QUrl m_currentSoundUrl;
    int m_playbackSerial = 0;
};
