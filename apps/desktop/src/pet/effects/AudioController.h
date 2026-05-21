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
    // 在加载新 manifest 时调用一次，注入皮肤声明的可选语音语言列表。
    void setAudioDefinition(const AudioDefinition &audio);

    // 静音开关：muted=true 时所有 playSound 请求都被丢弃，但状态字段仍维护。
    bool muted() const { return m_muted; }
    bool toggleMuted();

    // 当前选中的语音语言 id。空字符串表示未声明 / 未选择。
    QString currentLanguageId() const { return m_currentLanguageId; }
    QVariantList availableLanguages() const;
    bool setLanguage(const QString &languageId);

    // 最近一次成功播放的音效 URL；每次播放都会递增 playbackSerial，便于 QML 重播。
    QUrl currentSoundUrl() const { return m_currentSoundUrl; }
    int playbackSerial() const { return m_playbackSerial; }
    bool clearCurrentSound();

    // 根据当前语音语言从 RecipeDefinition.soundUrls 里挑出对应 URL（或回退到 soundUrl）。
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
