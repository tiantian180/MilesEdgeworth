#pragma once

#include "SettingsService.h"

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QtQml/qqmlregistration.h>

class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)
    Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY maxTokensChanged)
    Q_PROPERTY(int msPerChar READ msPerChar WRITE setMsPerChar NOTIFY msPerCharChanged)
    Q_PROPERTY(bool secretStoreAvailable READ secretStoreAvailable CONSTANT)
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    explicit SettingsController(SettingsService *service, QObject *parent = nullptr);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &value);

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &value);

    QString model() const { return m_model; }
    void setModel(const QString &value);

    double temperature() const { return m_temperature; }
    void setTemperature(double value);

    int maxTokens() const { return m_maxTokens; }
    void setMaxTokens(int value);

    int msPerChar() const { return m_msPerChar; }
    void setMsPerChar(int value);

    bool secretStoreAvailable() const;
    bool windowVisible() const { return m_windowVisible; }

    Q_INVOKABLE void openWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void save();
    Q_INVOKABLE void revert();

signals:
    void baseUrlChanged();
    void apiKeyChanged();
    void modelChanged();
    void temperatureChanged();
    void maxTokensChanged();
    void msPerCharChanged();
    void windowVisibleChanged();
    void saved();

private:
    void syncFromService(bool includeSecret);
    void setWindowVisible(bool visible);

    SettingsService *m_service = nullptr;
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;
    double m_temperature = 0.7;
    int m_maxTokens = 2048;
    int m_msPerChar = 80;
    bool m_apiKeyLoaded = false;
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
