#include "SettingsService.h"

#include "settings/SettingsLogging.h"

#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace {
ProviderConfigFile::ModelConfig defaultConfig()
{
    return {};
}
} // namespace

SettingsService::SettingsService(QString configPath, QObject *parent)
    : QObject(parent)
    , m_configFile(std::move(configPath))
{
    const auto status = m_configFile.load();
    if (status == ProviderConfigFile::LoadStatus::ParseError
        || status == ProviderConfigFile::LoadStatus::PermissionError) {
        qCWarning(settingsLog).noquote() << "provider config load failed"
                                         << QStringLiteral("path=%1").arg(m_configFile.path())
                                         << QStringLiteral("error=%1").arg(m_configFile.lastError());
    }
}

QStringList SettingsService::configNames() const
{
    return m_configFile.configNames();
}

QString SettingsService::activeModelConfig() const
{
    return m_configFile.activeModelConfig();
}

void SettingsService::setActiveModelConfig(const QString &name)
{
    m_configFile.setActiveModelConfig(name);
}

ProviderConfigFile::ModelConfig SettingsService::modelConfig(const QString &name) const
{
    return m_configFile.config(name);
}

bool SettingsService::setModelConfig(const QString &name, const ProviderConfigFile::ModelConfig &cfg)
{
    const bool ok = m_configFile.setConfig(name, cfg);
    if (ok) {
        m_configFile.setActiveModelConfig(cfg.name);
    }
    return ok;
}

void SettingsService::removeModelConfig(const QString &name)
{
    m_configFile.removeConfig(name);
}

QString SettingsService::baseUrl() const
{
    return activeConfig().baseUrl;
}

void SettingsService::setBaseUrl(const QString &value)
{
    auto cfg = activeConfig();
    cfg.baseUrl = value.trimmed();
    updateActiveConfig(cfg);
}

QString SettingsService::apiKey() const
{
    return activeConfig().apiKey;
}

void SettingsService::setApiKey(const QString &value)
{
    auto cfg = activeConfig();
    cfg.apiKey = value.trimmed();
    updateActiveConfig(cfg);
}

QString SettingsService::model() const
{
    return activeConfig().model;
}

void SettingsService::setModel(const QString &value)
{
    auto cfg = activeConfig();
    cfg.model = value.trimmed();
    updateActiveConfig(cfg);
}

std::optional<double> SettingsService::temperature() const
{
    return activeConfig().temperature;
}

void SettingsService::setTemperature(std::optional<double> value)
{
    if (value.has_value()) {
        *value = std::clamp(*value, 0.0, 2.0);
    }
    auto cfg = activeConfig();
    cfg.temperature = value;
    updateActiveConfig(cfg);
}

std::optional<int> SettingsService::maxTokens() const
{
    return activeConfig().maxTokens;
}

void SettingsService::setMaxTokens(std::optional<int> value)
{
    if (value.has_value() && *value < 1) {
        *value = 1;
    }
    auto cfg = activeConfig();
    cfg.maxTokens = value;
    updateActiveConfig(cfg);
}

int SettingsService::msPerChar() const
{
    return m_configFile.msPerChar();
}

void SettingsService::setMsPerChar(int value)
{
    m_configFile.setMsPerChar(value);
}

bool SettingsService::providerConfigured() const
{
    return !baseUrl().isEmpty() && !apiKey().isEmpty() && !model().isEmpty();
}

QString SettingsService::configPath() const
{
    return m_configFile.path();
}

QString SettingsService::lastError() const
{
    return m_configFile.lastError();
}

bool SettingsService::save()
{
    qCDebug(settingsLog).noquote() << "settings save requested"
                                   << QStringLiteral("path=%1").arg(m_configFile.path())
                                   << QStringLiteral("activeModelConfig=%1").arg(m_configFile.activeModelConfig())
                                   << QStringLiteral("providerConfigured=%1").arg(providerConfigured() ? "true" : "false");
    if (!m_configFile.save()) {
        qCWarning(settingsLog).noquote() << "settings save failed"
                                         << QStringLiteral("error=%1").arg(m_configFile.lastError());
        return false;
    }

    emit saved();
    qCDebug(settingsLog).noquote() << "settings save completed";
    return true;
}

ProviderConfigFile::ModelConfig SettingsService::activeConfig() const
{
    const QString active = m_configFile.activeModelConfig();
    if (active.isEmpty()) {
        return defaultConfig();
    }
    return m_configFile.config(active);
}

void SettingsService::updateActiveConfig(const ProviderConfigFile::ModelConfig &cfg)
{
    if (cfg.name.trimmed().isEmpty()) {
        return;
    }
    m_configFile.setConfig(m_configFile.activeModelConfig(), cfg);
}
