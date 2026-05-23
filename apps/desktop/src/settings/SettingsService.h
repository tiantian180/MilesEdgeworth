#pragma once

#include <QObject>
#include <QString>

class SecretStore;

// SettingsService owns persisted user-facing provider settings. Non-secret
// fields use QSettings; the API key is routed through the injected SecretStore.
class SettingsService : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *kKeychainService = "dev.tian.MilesEdgeworth.v2";
    static constexpr const char *kKeychainAccount = "MILES_PROVIDER_API_KEY";

    explicit SettingsService(SecretStore *secretStore, QObject *parent = nullptr);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &value);

    QString model() const { return m_model; }
    void setModel(const QString &value);

    double temperature() const { return m_temperature; }
    void setTemperature(double value);

    int maxTokens() const { return m_maxTokens; }
    void setMaxTokens(int value);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    QString apiKey();
    void setApiKey(const QString &value);

    bool secretStoreAvailable() const;
    bool save();

signals:
    void saved();

private:
    void load();
    void ensureApiKeyLoaded();

    SecretStore *m_secretStore = nullptr;
    QString m_baseUrl;
    QString m_model;
    double m_temperature = 0.7;
    int m_maxTokens = 2048;
    int m_msPerChar = 80;
    QString m_apiKey;
    QString m_savedApiKey;
    bool m_apiKeyLoaded = false;
};
