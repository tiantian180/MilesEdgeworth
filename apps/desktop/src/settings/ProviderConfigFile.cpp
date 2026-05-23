#include "ProviderConfigFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int kDefaultMsPerChar = 80;
constexpr int kMinMsPerChar = 40;
constexpr int kMaxMsPerChar = 200;

QString defaultConfigPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::home().filePath(QStringLiteral(".MilesEdgeworth"));
    }
    return QDir(dir).filePath(QStringLiteral("settings.json"));
}

int clampMsPerChar(int value)
{
    return std::clamp(value, kMinMsPerChar, kMaxMsPerChar);
}

QFileDevice::Permissions ownerOnlyPermissions()
{
    return QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ReadUser | QFileDevice::WriteUser;
}

QJsonObject withoutKnownFields(const QJsonObject &object, const QSet<QString> &knownFields)
{
    QJsonObject extra;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!knownFields.contains(it.key())) {
            extra.insert(it.key(), it.value());
        }
    }
    return extra;
}

QString backupPathFor(const QString &path)
{
    QFileInfo info(path);
    if (info.fileName() == QStringLiteral("settings.json")) {
        return info.dir().filePath(QStringLiteral("settings.json.bak"));
    }
    return path + QStringLiteral(".bak");
}

std::optional<double> optionalTemperature(const QJsonObject &object)
{
    const auto value = object.value(QStringLiteral("temperature"));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double temperature = value.toDouble();
    if (temperature < 0.0 || temperature > 2.0) {
        return std::nullopt;
    }
    return temperature;
}

std::optional<int> optionalMaxTokens(const QJsonObject &object)
{
    const auto value = object.value(QStringLiteral("maxTokens"));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double raw = value.toDouble();
    if (raw < 1.0 || std::floor(raw) != raw) {
        return std::nullopt;
    }
    return static_cast<int>(raw);
}

ProviderConfigFile::ModelConfig modelConfigFromJson(const QJsonObject &object)
{
    static const QSet<QString> knownFields {
        QStringLiteral("name"),
        QStringLiteral("baseUrl"),
        QStringLiteral("apiKey"),
        QStringLiteral("model"),
        QStringLiteral("temperature"),
        QStringLiteral("maxTokens"),
    };

    ProviderConfigFile::ModelConfig cfg;
    cfg.name = object.value(QStringLiteral("name")).toString().trimmed();
    cfg.baseUrl = object.value(QStringLiteral("baseUrl")).toString().trimmed();
    cfg.apiKey = object.value(QStringLiteral("apiKey")).toString().trimmed();
    cfg.model = object.value(QStringLiteral("model")).toString().trimmed();
    cfg.temperature = optionalTemperature(object);
    cfg.maxTokens = optionalMaxTokens(object);
    cfg.extraFields = withoutKnownFields(object, knownFields);
    return cfg;
}

QJsonObject modelConfigToJson(const ProviderConfigFile::ModelConfig &cfg)
{
    QJsonObject object = cfg.extraFields;
    object.insert(QStringLiteral("name"), cfg.name);
    object.insert(QStringLiteral("baseUrl"), cfg.baseUrl);
    object.insert(QStringLiteral("apiKey"), cfg.apiKey);
    object.insert(QStringLiteral("model"), cfg.model);
    if (cfg.temperature.has_value()) {
        object.insert(QStringLiteral("temperature"), *cfg.temperature);
    } else {
        object.remove(QStringLiteral("temperature"));
    }
    if (cfg.maxTokens.has_value()) {
        object.insert(QStringLiteral("maxTokens"), *cfg.maxTokens);
    } else {
        object.remove(QStringLiteral("maxTokens"));
    }
    return object;
}
} // namespace

ProviderConfigFile::ProviderConfigFile(QString path)
    : m_path(path.trimmed().isEmpty() ? defaultConfigPath() : std::move(path))
{
}

ProviderConfigFile::LoadStatus ProviderConfigFile::load()
{
    m_lastError.clear();
    m_rootExtraFields = {};
    m_chatExtraFields = {};
    m_configs.clear();
    m_activeModelConfig.clear();
    m_msPerChar = kDefaultMsPerChar;

    QFile file(m_path);
    if (!file.exists()) {
        return LoadStatus::FileNotFound;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = file.errorString();
        return LoadStatus::PermissionError;
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString backupPath = backupPathFor(m_path);
        QFile::remove(backupPath);
        bool backedUp = QFile::rename(m_path, backupPath);
        if (!backedUp && QFile::copy(m_path, backupPath)) {
            backedUp = QFile::remove(m_path);
        }
        m_lastError = backedUp
            ? QStringLiteral("配置文件损坏，已备份为 %1，请检查或重新配置。").arg(QFileInfo(backupPath).fileName())
            : QStringLiteral("配置文件损坏，且备份失败：%1").arg(parseError.errorString());
        return LoadStatus::ParseError;
    }

    static const QSet<QString> knownRootFields {
        QStringLiteral("schemaVersion"),
        QStringLiteral("activeModelConfig"),
        QStringLiteral("chat"),
        QStringLiteral("msPerChar"),
        QStringLiteral("modelConfigs"),
    };
    static const QSet<QString> knownChatFields {
        QStringLiteral("msPerChar"),
    };

    const auto root = doc.object();
    m_rootExtraFields = withoutKnownFields(root, knownRootFields);
    m_activeModelConfig = root.value(QStringLiteral("activeModelConfig")).toString().trimmed();
    const auto chat = root.value(QStringLiteral("chat")).toObject();
    m_chatExtraFields = withoutKnownFields(chat, knownChatFields);
    m_msPerChar = clampMsPerChar(chat.value(QStringLiteral("msPerChar"))
                                     .toInt(root.value(QStringLiteral("msPerChar")).toInt(kDefaultMsPerChar)));

    const auto configs = root.value(QStringLiteral("modelConfigs")).toArray();
    for (const auto &value : configs) {
        if (!value.isObject()) {
            continue;
        }
        auto cfg = modelConfigFromJson(value.toObject());
        if (cfg.name.isEmpty() || configIndex(cfg.name) != -1) {
            continue;
        }
        m_configs.append(cfg);
    }

    selectFallbackActiveConfig();
    return LoadStatus::Ok;
}

bool ProviderConfigFile::save()
{
    m_lastError.clear();

    QDir dir = QFileInfo(m_path).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Could not create config directory: %1").arg(dir.absolutePath());
        return false;
    }

    QJsonArray configs;
    for (const auto &cfg : m_configs) {
        configs.append(modelConfigToJson(cfg));
    }

    QJsonObject root = m_rootExtraFields;
    QJsonObject chat = m_chatExtraFields;
    chat.insert(QStringLiteral("msPerChar"), m_msPerChar);

    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("activeModelConfig"), m_activeModelConfig);
    root.insert(QStringLiteral("chat"), chat);
    root.insert(QStringLiteral("modelConfigs"), configs);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = file.errorString();
        return false;
    }
    file.setPermissions(ownerOnlyPermissions());
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_lastError = file.errorString();
        return false;
    }

    QFile saved(m_path);
    saved.setPermissions(ownerOnlyPermissions());
    return true;
}

QString ProviderConfigFile::path() const
{
    return m_path;
}

QString ProviderConfigFile::lastError() const
{
    return m_lastError;
}

QString ProviderConfigFile::activeModelConfig() const
{
    return m_activeModelConfig;
}

void ProviderConfigFile::setActiveModelConfig(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (configIndex(trimmed) == -1) {
        return;
    }
    m_activeModelConfig = trimmed;
}

QStringList ProviderConfigFile::configNames() const
{
    QStringList names;
    names.reserve(m_configs.size());
    for (const auto &cfg : m_configs) {
        names.append(cfg.name);
    }
    return names;
}

ProviderConfigFile::ModelConfig ProviderConfigFile::config(const QString &name) const
{
    const int index = configIndex(name.trimmed());
    if (index == -1) {
        return {};
    }
    return m_configs.at(index);
}

bool ProviderConfigFile::setConfig(const QString &name, const ModelConfig &cfg)
{
    const QString oldName = name.trimmed();
    const QString newName = cfg.name.trimmed();
    if (oldName.isEmpty() || newName.isEmpty()) {
        return false;
    }

    const int oldIndex = configIndex(oldName);
    const int newIndex = configIndex(newName);
    if (newIndex != -1 && newIndex != oldIndex) {
        return false;
    }
    if (oldIndex == -1 && oldName != newName) {
        return false;
    }

    ModelConfig normalized = cfg;
    normalized.name = newName;
    normalized.baseUrl = normalized.baseUrl.trimmed();
    normalized.apiKey = normalized.apiKey.trimmed();
    normalized.model = normalized.model.trimmed();

    if (oldIndex == -1) {
        m_configs.append(normalized);
    } else {
        m_configs[oldIndex] = normalized;
        if (m_activeModelConfig == oldName) {
            m_activeModelConfig = newName;
        }
    }

    if (m_activeModelConfig.isEmpty()) {
        m_activeModelConfig = newName;
    }
    return true;
}

void ProviderConfigFile::removeConfig(const QString &name)
{
    const QString trimmed = name.trimmed();
    const int index = configIndex(trimmed);
    if (index == -1) {
        return;
    }
    m_configs.removeAt(index);
    if (m_activeModelConfig == trimmed) {
        selectFallbackActiveConfig();
    }
}

void ProviderConfigFile::moveConfig(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_configs.size()
        || toIndex < 0 || toIndex >= m_configs.size()
        || fromIndex == toIndex) {
        return;
    }
    m_configs.move(fromIndex, toIndex);
}

int ProviderConfigFile::msPerChar() const
{
    return m_msPerChar;
}

void ProviderConfigFile::setMsPerChar(int value)
{
    m_msPerChar = clampMsPerChar(value);
}

int ProviderConfigFile::configIndex(const QString &name) const
{
    for (int i = 0; i < m_configs.size(); ++i) {
        if (m_configs.at(i).name == name) {
            return i;
        }
    }
    return -1;
}

void ProviderConfigFile::selectFallbackActiveConfig()
{
    if (configIndex(m_activeModelConfig) != -1) {
        return;
    }
    m_activeModelConfig = m_configs.isEmpty() ? QString() : m_configs.first().name;
}
