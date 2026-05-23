#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

class ProviderConfigFile
{
public:
    enum class LoadStatus { Ok, FileNotFound, ParseError, PermissionError };

    struct ModelConfig {
        QString name;
        QString baseUrl;
        QString apiKey;
        QString model;
        std::optional<double> temperature;
        std::optional<int> maxTokens;
        QJsonObject extraFields;
    };

    explicit ProviderConfigFile(QString path = {});

    LoadStatus load();
    bool save();
    QString path() const;
    QString lastError() const;

    QString activeModelConfig() const;
    void setActiveModelConfig(const QString &name);

    QStringList configNames() const;
    ModelConfig config(const QString &name) const;
    bool setConfig(const QString &name, const ModelConfig &cfg);
    void removeConfig(const QString &name);
    void moveConfig(int fromIndex, int toIndex);

    int msPerChar() const;
    void setMsPerChar(int value);

private:
    int configIndex(const QString &name) const;
    void selectFallbackActiveConfig();

    QString m_path;
    QString m_activeModelConfig;
    QString m_lastError;
    QList<ModelConfig> m_configs;
    int m_msPerChar = 80;
    QJsonObject m_rootExtraFields;
};
