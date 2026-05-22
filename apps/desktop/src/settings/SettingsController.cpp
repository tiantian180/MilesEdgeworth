#include "SettingsController.h"

#include "pet/PetRuntime.h"
#include "pet/manifest/PersonaStore.h"

#include <QtGlobal>

SettingsController::SettingsController(SettingsService *service, PetRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_runtime(runtime)
{
    syncFromService(false);
    reloadPersona();
}

void SettingsController::syncFromService(bool includeSecret)
{
    if (m_service == nullptr) {
        return;
    }

    m_baseUrl = m_service->baseUrl();
    m_model = m_service->model();
    m_temperature = m_service->temperature();
    m_maxTokens = m_service->maxTokens();
    m_msPerChar = m_service->msPerChar();
    if (includeSecret) {
        m_apiKey = m_service->apiKey();
        m_apiKeyLoaded = true;
    }

    emit baseUrlChanged();
    if (includeSecret) {
        emit apiKeyChanged();
    }
    emit modelChanged();
    emit temperatureChanged();
    emit maxTokensChanged();
    emit msPerCharChanged();
}

void SettingsController::setBaseUrl(const QString &value)
{
    if (m_baseUrl == value) {
        return;
    }
    m_baseUrl = value;
    emit baseUrlChanged();
}

void SettingsController::setApiKey(const QString &value)
{
    if (m_apiKey == value) {
        return;
    }
    m_apiKey = value;
    m_apiKeyLoaded = true;
    emit apiKeyChanged();
}

void SettingsController::setModel(const QString &value)
{
    if (m_model == value) {
        return;
    }
    m_model = value;
    emit modelChanged();
}

void SettingsController::setTemperature(double value)
{
    if (value < 0.0) {
        value = 0.0;
    }
    if (value > 2.0) {
        value = 2.0;
    }
    if (qFuzzyCompare(m_temperature + 1.0, value + 1.0)) {
        return;
    }
    m_temperature = value;
    emit temperatureChanged();
}

void SettingsController::setMaxTokens(int value)
{
    if (value < 1) {
        value = 1;
    }
    if (value > 32768) {
        value = 32768;
    }
    if (m_maxTokens == value) {
        return;
    }
    m_maxTokens = value;
    emit maxTokensChanged();
}

void SettingsController::setMsPerChar(int value)
{
    if (value < 40) {
        value = 40;
    }
    if (value > 200) {
        value = 200;
    }
    if (m_msPerChar == value) {
        return;
    }
    m_msPerChar = value;
    emit msPerCharChanged();
}

void SettingsController::setPersonaPrompt(const QString &value)
{
    if (m_personaPrompt == value) {
        return;
    }
    m_personaPrompt = value;
    emit personaPromptChanged();
}

bool SettingsController::secretStoreAvailable() const
{
    return m_service != nullptr && m_service->secretStoreAvailable();
}

void SettingsController::openWindow()
{
    syncFromService(true);
    reloadPersona();
    setWindowVisible(true);
}

void SettingsController::closeWindow()
{
    setWindowVisible(false);
}

void SettingsController::save()
{
    if (m_service == nullptr) {
        return;
    }

    m_service->setBaseUrl(m_baseUrl);
    if (m_apiKeyLoaded) {
        m_service->setApiKey(m_apiKey);
    }
    m_service->setModel(m_model);
    m_service->setTemperature(m_temperature);
    m_service->setMaxTokens(m_maxTokens);
    m_service->setMsPerChar(m_msPerChar);
    m_service->save();

    if (m_runtime != nullptr) {
        QString error;
        if (!PersonaStore::writeForManifest(m_runtime->manifest(), m_personaPrompt, &error)) {
            setPersonaError(error);
            return;
        }
        if (!m_runtime->reloadActiveSkin()) {
            setPersonaError(QStringLiteral("保存成功，但重新加载当前皮肤失败。"));
            return;
        }
        reloadPersona();
    }

    emit saved();
    closeWindow();
}

void SettingsController::revert()
{
    syncFromService(true);
    reloadPersona();
    closeWindow();
}

void SettingsController::reloadPersona()
{
    if (m_runtime == nullptr) {
        setPersonaPrompt(QString());
        setPersonaError(QString());
        return;
    }

    setPersonaPrompt(m_runtime->manifest().personaPrompt);
    setPersonaError(QString());
}

void SettingsController::setPersonaError(const QString &value)
{
    if (m_personaError == value) {
        return;
    }
    m_personaError = value;
    emit personaErrorChanged();
}

void SettingsController::setWindowVisible(bool visible)
{
    if (m_windowVisible == visible) {
        return;
    }
    m_windowVisible = visible;
    emit windowVisibleChanged();
}
