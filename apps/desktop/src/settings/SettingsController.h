#pragma once

#include "SettingsService.h"

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class PetRuntime;

class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList configNames READ configNames NOTIFY configNamesChanged)
    Q_PROPERTY(QString activeModelConfig READ activeModelConfig NOTIFY activeModelConfigChanged)
    Q_PROPERTY(QString configName READ configName WRITE setConfigName NOTIFY configNameChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString temperatureText READ temperatureText WRITE setTemperatureText NOTIFY temperatureTextChanged)
    Q_PROPERTY(QString maxTokensText READ maxTokensText WRITE setMaxTokensText NOTIFY maxTokensTextChanged)
    Q_PROPERTY(int msPerChar READ msPerChar WRITE setMsPerChar NOTIFY msPerCharChanged)
    Q_PROPERTY(bool providerConfigured READ providerConfigured NOTIFY providerConfiguredChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationErrorChanged)
    Q_PROPERTY(QString personaPrompt READ personaPrompt WRITE setPersonaPrompt NOTIFY personaPromptChanged)
    Q_PROPERTY(QString personaError READ personaError NOTIFY personaErrorChanged)
    Q_PROPERTY(QString saveError READ saveError NOTIFY saveErrorChanged)
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    explicit SettingsController(SettingsService *service, PetRuntime *runtime = nullptr, QObject *parent = nullptr);

    QStringList configNames() const { return m_configNames; }
    QString activeModelConfig() const { return m_activeModelConfig; }

    QString configName() const { return m_configName; }
    void setConfigName(const QString &value);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &value);

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &value);

    QString model() const { return m_model; }
    void setModel(const QString &value);

    QString temperatureText() const { return m_temperatureText; }
    void setTemperatureText(const QString &value);

    QString maxTokensText() const { return m_maxTokensText; }
    void setMaxTokensText(const QString &value);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    bool providerConfigured() const { return m_providerConfigured; }
    QString validationError() const { return m_validationError; }

    QString personaPrompt() const { return m_personaPrompt; }
    QString personaError() const { return m_personaError; }
    QString saveError() const { return m_saveError; }
    void setPersonaPrompt(const QString &value);

    bool windowVisible() const { return m_windowVisible; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void save();
    Q_INVOKABLE void revert();
    Q_INVOKABLE void reloadPersona();
    Q_INVOKABLE void selectConfig(const QString &name);
    Q_INVOKABLE void addConfig();
    Q_INVOKABLE void deleteConfig(const QString &name = {});

signals:
    void configNamesChanged();
    void activeModelConfigChanged();
    void configNameChanged();
    void baseUrlChanged();
    void apiKeyChanged();
    void modelChanged();
    void temperatureTextChanged();
    void maxTokensTextChanged();
    void msPerCharChanged();
    void providerConfiguredChanged();
    void validationErrorChanged();
    void personaPromptChanged();
    void personaErrorChanged();
    void saveErrorChanged();
    void windowVisibleChanged();
    void saved();

private:
    void syncFromService();
    void syncFromConfig(const ProviderConfigFile::ModelConfig &cfg);
    void updateProviderConfigured();
    void setValidationError(const QString &value);
    void setPersonaError(const QString &value);
    void setSaveError(const QString &value);
    void setWindowVisible(bool visible);

    SettingsService *m_service = nullptr;
    PetRuntime *m_runtime = nullptr;
    QStringList m_configNames;
    QString m_activeModelConfig;
    QString m_configName;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    QString m_temperatureText;
    QString m_maxTokensText;
    QString m_personaPrompt;
    QString m_validationError;
    QString m_personaError;
    QString m_saveError;
    int m_msPerChar = 80;
    bool m_providerConfigured = false;
    bool m_windowVisible = false;
};

struct SettingsControllerForeign
{
    Q_GADGET
    QML_FOREIGN(SettingsController)
    QML_NAMED_ELEMENT(SettingsController)
    QML_SINGLETON

public:
    inline static SettingsController *s_instance = nullptr;

    static SettingsController *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
