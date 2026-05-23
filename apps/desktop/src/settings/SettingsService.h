#pragma once

#include "ProviderConfigFile.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

// SettingsService owns user-facing provider settings stored in settings.json.
class SettingsService : public QObject
{
    Q_OBJECT

public:
    explicit SettingsService(QString configPath = {}, QObject *parent = nullptr);

    QStringList configNames() const;

    QString activeModelConfig() const;
    void setActiveModelConfig(const QString &name);

    ProviderConfigFile::ModelConfig modelConfig(const QString &name) const;
    bool setModelConfig(const QString &name, const ProviderConfigFile::ModelConfig &cfg);
    void removeModelConfig(const QString &name);

    QString baseUrl() const;
    void setBaseUrl(const QString &value);

    QString apiKey() const;
    void setApiKey(const QString &value);

    QString model() const;
    void setModel(const QString &value);

    std::optional<double> temperature() const;
    void setTemperature(std::optional<double> value);

    std::optional<int> maxTokens() const;
    void setMaxTokens(std::optional<int> value);

    int msPerChar() const;
    void setMsPerChar(int value);

    bool providerConfigured() const;
    QString lastError() const;
    bool save();

signals:
    void saved();

private:
    ProviderConfigFile::ModelConfig activeConfig() const;
    void updateActiveConfig(const ProviderConfigFile::ModelConfig &cfg);

    ProviderConfigFile m_configFile;
};
