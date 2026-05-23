#include "SettingsService.h"

#include "SecretStore.h"

#include <QSettings>

namespace {
constexpr const char *kBaseUrlKey = "provider/baseUrl";
constexpr const char *kModelKey = "provider/model";
constexpr const char *kTemperatureKey = "provider/temperature";
constexpr const char *kMaxTokensKey = "provider/maxTokens";
constexpr const char *kMsPerCharKey = "chat/msPerChar";
} // namespace

SettingsService::SettingsService(SecretStore *secretStore, QObject *parent)
    : QObject(parent)
    , m_secretStore(secretStore)
{
    load();
}

void SettingsService::load()
{
    QSettings settings;
    m_baseUrl = settings.value(QString::fromLatin1(kBaseUrlKey)).toString();
    m_model = settings.value(QString::fromLatin1(kModelKey)).toString();
    m_temperature = settings.value(QString::fromLatin1(kTemperatureKey), 0.7).toDouble();
    m_maxTokens = settings.value(QString::fromLatin1(kMaxTokensKey), 2048).toInt();
    m_msPerChar = settings.value(QString::fromLatin1(kMsPerCharKey), 80).toInt();

    m_savedApiKey.clear();
    m_apiKey.clear();
    m_apiKeyLoaded = false;
}

void SettingsService::ensureApiKeyLoaded()
{
    if (m_apiKeyLoaded) {
        return;
    }

    if (m_secretStore != nullptr && m_secretStore->available()) {
        m_savedApiKey = m_secretStore->read(QString::fromUtf8(kKeychainService),
                                            QString::fromUtf8(kKeychainAccount));
        m_apiKey = m_savedApiKey;
    } else {
        m_savedApiKey.clear();
        m_apiKey.clear();
    }

    m_apiKeyLoaded = true;
}

void SettingsService::setBaseUrl(const QString &value)
{
    m_baseUrl = value.trimmed();
}

void SettingsService::setModel(const QString &value)
{
    m_model = value.trimmed();
}

void SettingsService::setTemperature(double value)
{
    if (value < 0.0) {
        value = 0.0;
    }
    if (value > 2.0) {
        value = 2.0;
    }
    m_temperature = value;
}

void SettingsService::setMaxTokens(int value)
{
    if (value < 1) {
        value = 1;
    }
    if (value > 32768) {
        value = 32768;
    }
    m_maxTokens = value;
}

void SettingsService::setMsPerChar(int value)
{
    if (value < 40) {
        value = 40;
    }
    if (value > 200) {
        value = 200;
    }
    m_msPerChar = value;
}

void SettingsService::setApiKey(const QString &value)
{
    m_apiKey = value;
    m_apiKeyLoaded = true;
}

QString SettingsService::apiKey()
{
    ensureApiKeyLoaded();
    return m_apiKey;
}

bool SettingsService::secretStoreAvailable() const
{
    return m_secretStore != nullptr && m_secretStore->available();
}

bool SettingsService::save()
{
    {
        QSettings settings;
        settings.setValue(QString::fromLatin1(kBaseUrlKey), m_baseUrl);
        settings.setValue(QString::fromLatin1(kModelKey), m_model);
        settings.setValue(QString::fromLatin1(kTemperatureKey), m_temperature);
        settings.setValue(QString::fromLatin1(kMaxTokensKey), m_maxTokens);
        settings.setValue(QString::fromLatin1(kMsPerCharKey), m_msPerChar);
    }

    if (m_apiKeyLoaded && m_apiKey != m_savedApiKey && m_secretStore != nullptr && m_secretStore->available()) {
        bool secretSaved = false;
        if (m_apiKey.isEmpty()) {
            secretSaved = m_secretStore->remove(QString::fromUtf8(kKeychainService),
                                                QString::fromUtf8(kKeychainAccount));
        } else {
            secretSaved = m_secretStore->write(QString::fromUtf8(kKeychainService),
                                               QString::fromUtf8(kKeychainAccount),
                                               m_apiKey);
        }
        if (!secretSaved) {
            return false;
        }
        m_savedApiKey = m_apiKey;
    }

    emit saved();
    return true;
}
