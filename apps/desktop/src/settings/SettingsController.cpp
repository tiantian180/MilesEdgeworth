#include "SettingsController.h"

#include "pet/PetRuntime.h"
#include "pet/manifest/PersonaStore.h"
#include "settings/SettingsLogging.h"

#include <QtGlobal>

#include <optional>

namespace {
QString temperatureToText(std::optional<double> value)
{
    return value.has_value() ? QString::number(*value, 'f', 2) : QString();
}

QString maxTokensToText(std::optional<int> value)
{
    return value.has_value() ? QString::number(*value) : QString();
}

std::optional<double> parseTemperature(const QString &text, QString *error)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    if (!ok || value < 0.0 || value > 2.0) {
        *error = QStringLiteral("Temperature 必须留空，或填写 0 到 2 之间的数字。");
        return std::nullopt;
    }
    return value;
}

std::optional<int> parseMaxTokens(const QString &text, QString *error)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    const int value = trimmed.toInt(&ok);
    if (!ok || value < 1) {
        *error = QStringLiteral("Max Tokens 必须留空，或填写正整数。");
        return std::nullopt;
    }
    return value;
}

QString nextConfigName(const QStringList &names)
{
    const QString base = QStringLiteral("新配置");
    if (!names.contains(base)) {
        return base;
    }
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("新配置 %1").arg(i);
        if (!names.contains(candidate)) {
            return candidate;
        }
    }
    return QStringLiteral("新配置 1000");
}
} // namespace

SettingsController::SettingsController(SettingsService *service, PetRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_runtime(runtime)
{
    syncFromService();
    reloadPersona();
}

void SettingsController::syncFromService()
{
    if (m_service == nullptr) {
        return;
    }

    m_configNames = m_service->configNames();
    m_activeModelConfig = m_service->activeModelConfig();
    syncFromConfig(m_service->modelConfig(m_activeModelConfig));
    m_msPerChar = m_service->msPerChar();

    emit configNamesChanged();
    emit activeModelConfigChanged();
    emit msPerCharChanged();
}

void SettingsController::syncFromConfig(const ProviderConfigFile::ModelConfig &cfg)
{
    m_configName = cfg.name;
    m_baseUrl = cfg.baseUrl;
    m_apiKey = cfg.apiKey;
    m_model = cfg.model;
    m_temperatureText = temperatureToText(cfg.temperature);
    m_maxTokensText = maxTokensToText(cfg.maxTokens);

    emit configNameChanged();
    emit baseUrlChanged();
    emit apiKeyChanged();
    emit modelChanged();
    emit temperatureTextChanged();
    emit maxTokensTextChanged();
    updateProviderConfigured();
}

void SettingsController::setConfigName(const QString &value)
{
    if (m_configName == value) {
        return;
    }
    m_configName = value;
    emit configNameChanged();
}

void SettingsController::setBaseUrl(const QString &value)
{
    if (m_baseUrl == value) {
        return;
    }
    m_baseUrl = value;
    emit baseUrlChanged();
    updateProviderConfigured();
}

void SettingsController::setApiKey(const QString &value)
{
    if (m_apiKey == value) {
        return;
    }
    m_apiKey = value;
    emit apiKeyChanged();
    updateProviderConfigured();
}

void SettingsController::setModel(const QString &value)
{
    if (m_model == value) {
        return;
    }
    m_model = value;
    emit modelChanged();
    updateProviderConfigured();
}

void SettingsController::setTemperatureText(const QString &value)
{
    if (m_temperatureText == value) {
        return;
    }
    m_temperatureText = value;
    emit temperatureTextChanged();
}

void SettingsController::setMaxTokensText(const QString &value)
{
    if (m_maxTokensText == value) {
        return;
    }
    m_maxTokensText = value;
    emit maxTokensTextChanged();
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

void SettingsController::openWindow()
{
    syncFromService();
    reloadPersona();
    setValidationError(QString());
    setSaveError(QString());
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
    setValidationError(QString());
    setSaveError(QString());

    qCDebug(settingsLog).noquote() << "settings controller save requested"
                                   << QStringLiteral("configName=%1").arg(m_configName)
                                   << QStringLiteral("baseUrlSet=%1").arg(!m_baseUrl.trimmed().isEmpty() ? "true" : "false")
                                   << QStringLiteral("modelSet=%1").arg(!m_model.trimmed().isEmpty() ? "true" : "false");

    QString validation;
    const auto temperature = parseTemperature(m_temperatureText, &validation);
    if (!validation.isEmpty()) {
        setValidationError(validation);
        return;
    }
    const auto maxTokens = parseMaxTokens(m_maxTokensText, &validation);
    if (!validation.isEmpty()) {
        setValidationError(validation);
        return;
    }

    const bool hasProviderInput = !m_configName.trimmed().isEmpty()
        || !m_baseUrl.trimmed().isEmpty()
        || !m_apiKey.trimmed().isEmpty()
        || !m_model.trimmed().isEmpty()
        || !m_temperatureText.trimmed().isEmpty()
        || !m_maxTokensText.trimmed().isEmpty();

    ProviderConfigFile::ModelConfig cfg;
    if (hasProviderInput) {
        cfg.name = m_configName.trimmed();
        cfg.baseUrl = m_baseUrl.trimmed();
        cfg.apiKey = m_apiKey.trimmed();
        cfg.model = m_model.trimmed();
        cfg.temperature = temperature;
        cfg.maxTokens = maxTokens;

        if (cfg.name.isEmpty()) {
            setValidationError(QStringLiteral("请填写配置名称。"));
            return;
        }
        if (cfg.baseUrl.isEmpty() || cfg.apiKey.isEmpty() || cfg.model.isEmpty()) {
            setValidationError(QStringLiteral("Base URL、API Key 和 Model 都必须填写。"));
            return;
        }
    }

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

    if (hasProviderInput) {
        const QString oldName = m_activeModelConfig.trimmed().isEmpty() ? cfg.name : m_activeModelConfig;
        if (!m_service->setModelConfig(oldName, cfg)) {
            setValidationError(QStringLiteral("配置名称重复或无效。"));
            return;
        }
    }
    m_service->setMsPerChar(m_msPerChar);
    if (!m_service->save()) {
        const QString detail = m_service->lastError().trimmed();
        setSaveError(detail.isEmpty()
                ? QStringLiteral("保存模型配置失败。")
                : QStringLiteral("保存模型配置失败：%1").arg(detail));
        qCWarning(settingsLog).noquote() << "settings controller save failed";
        return;
    }

    syncFromService();
    qCDebug(settingsLog).noquote() << "settings controller save completed";
    emit saved();
    closeWindow();
}

void SettingsController::revert()
{
    syncFromService();
    reloadPersona();
    setValidationError(QString());
    setSaveError(QString());
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

void SettingsController::selectConfig(const QString &name)
{
    if (m_service == nullptr) {
        return;
    }
    m_service->setActiveModelConfig(name);
    syncFromService();
    setValidationError(QString());
}

void SettingsController::addConfig()
{
    m_activeModelConfig.clear();
    emit activeModelConfigChanged();
    ProviderConfigFile::ModelConfig cfg;
    cfg.name = nextConfigName(m_configNames);
    syncFromConfig(cfg);
    setValidationError(QString());
    setSaveError(QString());
}

void SettingsController::deleteConfig(const QString &name)
{
    if (m_service == nullptr) {
        return;
    }
    const QString target = name.trimmed().isEmpty() ? m_activeModelConfig : name.trimmed();
    if (target.isEmpty()) {
        return;
    }

    m_service->removeModelConfig(target);
    if (!m_service->save()) {
        const QString detail = m_service->lastError().trimmed();
        setSaveError(detail.isEmpty()
                ? QStringLiteral("删除模型配置失败。")
                : QStringLiteral("删除模型配置失败：%1").arg(detail));
        return;
    }
    syncFromService();
    emit saved();
}

void SettingsController::updateProviderConfigured()
{
    const bool configured = !m_baseUrl.trimmed().isEmpty()
        && !m_apiKey.trimmed().isEmpty()
        && !m_model.trimmed().isEmpty();
    if (m_providerConfigured == configured) {
        return;
    }
    m_providerConfigured = configured;
    emit providerConfiguredChanged();
}

void SettingsController::setValidationError(const QString &value)
{
    if (m_validationError == value) {
        return;
    }
    m_validationError = value;
    emit validationErrorChanged();
}

void SettingsController::setPersonaError(const QString &value)
{
    if (m_personaError == value) {
        return;
    }
    m_personaError = value;
    emit personaErrorChanged();
}

void SettingsController::setSaveError(const QString &value)
{
    if (m_saveError == value) {
        return;
    }
    m_saveError = value;
    emit saveErrorChanged();
}

void SettingsController::setWindowVisible(bool visible)
{
    if (m_windowVisible == visible) {
        return;
    }
    m_windowVisible = visible;
    emit windowVisibleChanged();
}
