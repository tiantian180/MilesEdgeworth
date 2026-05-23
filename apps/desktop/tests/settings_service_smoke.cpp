#include "settings/ProviderConfigFile.h"
#include "settings/SecretStore.h"
#include "settings/SettingsController.h"
#include "settings/SettingsService.h"
#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <cassert>
#include <map>
#include <utility>

namespace {
class InMemorySecretStore : public SecretStore
{
public:
    bool available() const override { return true; }

    QString read(const QString &service, const QString &account) override
    {
        ++m_readCount;
        const auto it = m_store.find({service, account});
        return it == m_store.end() ? QString() : it->second;
    }

    bool write(const QString &service, const QString &account, const QString &secret) override
    {
        if (secret.isEmpty()) {
            m_store.erase({service, account});
        } else {
            m_store[{service, account}] = secret;
        }
        return true;
    }

    bool remove(const QString &service, const QString &account) override
    {
        m_store.erase({service, account});
        return true;
    }

    QString peek(const QString &service, const QString &account) const
    {
        const auto it = m_store.find({service, account});
        return it == m_store.end() ? QString() : it->second;
    }

    int readCount() const { return m_readCount; }

private:
    std::map<std::pair<QString, QString>, QString> m_store;
    int m_readCount = 0;
};

class FailingSecretStore : public SecretStore
{
public:
    bool available() const override { return true; }
    QString read(const QString &, const QString &) override { return {}; }
    bool write(const QString &, const QString &, const QString &) override { return false; }
    bool remove(const QString &, const QString &) override { return false; }
};

void setupQSettingsScope(QTemporaryDir &dir)
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("tian-test"));
    QCoreApplication::setApplicationName(QStringLiteral("MilesEdgeworth-test"));
}

bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << content;
    return true;
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir tmp;
    assert(tmp.isValid());
    setupQSettingsScope(tmp);

    {
        const QString path = tmp.filePath(QStringLiteral("providers.json"));
        ProviderConfigFile file(path);
        assert(file.load() == ProviderConfigFile::LoadStatus::FileNotFound);
        assert(file.configNames().isEmpty());

        ProviderConfigFile::ModelConfig cfg;
        cfg.name = QStringLiteral("deepseek");
        cfg.baseUrl = QStringLiteral("https://api.deepseek.com");
        cfg.apiKey = QStringLiteral("sk-test");
        cfg.model = QStringLiteral("deepseek-chat");
        cfg.temperature = 0.7;
        cfg.maxTokens = std::nullopt;
        assert(file.setConfig(cfg.name, cfg));
        file.setActiveModelConfig(cfg.name);
        file.setMsPerChar(60);
        assert(file.save());

        ProviderConfigFile reopened(path);
        assert(reopened.load() == ProviderConfigFile::LoadStatus::Ok);
        assert(reopened.activeModelConfig() == QStringLiteral("deepseek"));
        assert(reopened.config(QStringLiteral("deepseek")).apiKey == QStringLiteral("sk-test"));
        assert(reopened.config(QStringLiteral("deepseek")).temperature.has_value());
        assert(!reopened.config(QStringLiteral("deepseek")).maxTokens.has_value());
        assert(reopened.msPerChar() == 60);
    }

    {
        const QString path = tmp.filePath(QStringLiteral("providers-invalid.json"));
        assert(writeFile(path, QStringLiteral("{not-json")));
        ProviderConfigFile file(path);
        assert(file.load() == ProviderConfigFile::LoadStatus::ParseError);
        assert(QFile::exists(path + QStringLiteral(".bak")));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("providers-extra.json"));
        assert(writeFile(path, QStringLiteral(R"JSON(
{
  "rootUnknown": "keep-root",
  "activeModelConfig": "deepseek",
  "msPerChar": 90,
  "modelConfigs": [
    {
      "name": "deepseek",
      "baseUrl": "https://api.deepseek.com",
      "apiKey": "sk-test",
      "model": "deepseek-chat",
      "temperature": 0.7,
      "providerUnknown": "keep-provider"
    }
  ]
}
)JSON")));

        ProviderConfigFile file(path);
        assert(file.load() == ProviderConfigFile::LoadStatus::Ok);
        auto cfg = file.config(QStringLiteral("deepseek"));
        assert(cfg.extraFields.value(QStringLiteral("providerUnknown")).toString() == QStringLiteral("keep-provider"));
        cfg.model = QStringLiteral("deepseek-reasoner");
        assert(file.setConfig(cfg.name, cfg));
        assert(file.save());

        QFile saved(path);
        assert(saved.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(saved.readAll());
        const auto root = doc.object();
        assert(root.value(QStringLiteral("rootUnknown")).toString() == QStringLiteral("keep-root"));
        const auto configs = root.value(QStringLiteral("modelConfigs")).toArray();
        assert(configs.size() == 1);
        const auto savedCfg = configs.at(0).toObject();
        assert(savedCfg.value(QStringLiteral("providerUnknown")).toString() == QStringLiteral("keep-provider"));
        assert(savedCfg.value(QStringLiteral("model")).toString() == QStringLiteral("deepseek-reasoner"));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("providers-delete.json"));
        ProviderConfigFile file(path);
        ProviderConfigFile::ModelConfig first;
        first.name = QStringLiteral("first");
        first.baseUrl = QStringLiteral("https://first.example.com");
        first.apiKey = QStringLiteral("sk-first");
        first.model = QStringLiteral("first-model");
        ProviderConfigFile::ModelConfig second = first;
        second.name = QStringLiteral("second");
        second.baseUrl = QStringLiteral("https://second.example.com");
        second.apiKey = QStringLiteral("sk-second");
        second.model = QStringLiteral("second-model");
        assert(file.setConfig(first.name, first));
        assert(file.setConfig(second.name, second));
        file.setActiveModelConfig(second.name);
        file.removeConfig(second.name);
        assert(file.activeModelConfig() == first.name);

        ProviderConfigFile::ModelConfig emptyName = first;
        emptyName.name.clear();
        assert(!file.setConfig(QString(), emptyName));

        ProviderConfigFile::ModelConfig duplicate = first;
        duplicate.name = first.name;
        assert(!file.setConfig(QStringLiteral("other"), duplicate));
    }

    {
        InMemorySecretStore store;
        SettingsService service(&store);

        assert(service.baseUrl().isEmpty());
        assert(service.model().isEmpty());
        assert(qFuzzyCompare(service.temperature() + 1.0, 0.7 + 1.0));
        assert(service.maxTokens() == 2048);
        assert(service.msPerChar() == 80);

        service.setBaseUrl(QStringLiteral("https://api.example.com"));
        service.setModel(QStringLiteral("gpt-test"));
        service.setTemperature(0.3);
        service.setMaxTokens(1024);
        service.setMsPerChar(120);
        service.save();
    }

    {
        InMemorySecretStore store;
        SettingsService service(&store);
        assert(service.baseUrl() == QStringLiteral("https://api.example.com"));
        assert(service.model() == QStringLiteral("gpt-test"));
        assert(qFuzzyCompare(service.temperature() + 1.0, 0.3 + 1.0));
        assert(service.maxTokens() == 1024);
        assert(service.msPerChar() == 120);
    }

    {
        InMemorySecretStore store;
        store.write(QString::fromUtf8(SettingsService::kKeychainService),
                    QString::fromUtf8(SettingsService::kKeychainAccount),
                    QStringLiteral("sk-secret"));
        SettingsService service(&store);
        assert(store.readCount() == 0);

        SettingsController controller(&service);
        assert(store.readCount() == 0);

        controller.openWindow();
        assert(store.readCount() == 0);
        assert(controller.apiKey().isEmpty());

        controller.openWindow();
        assert(store.readCount() == 0);
        controller.save();
        assert(store.peek(QString::fromUtf8(SettingsService::kKeychainService),
                          QString::fromUtf8(SettingsService::kKeychainAccount))
            == QStringLiteral("sk-secret"));

        controller.openWindow();
        controller.setApiKey(QStringLiteral("sk-replaced"));
        controller.save();
        assert(store.peek(QString::fromUtf8(SettingsService::kKeychainService),
                          QString::fromUtf8(SettingsService::kKeychainAccount))
            == QStringLiteral("sk-replaced"));
        assert(store.readCount() == 0);
    }

    {
        InMemorySecretStore store;
        SettingsService service(&store);
        assert(service.apiKey().isEmpty());

        service.setApiKey(QStringLiteral("sk-secret"));
        service.save();
        assert(service.apiKey() == QStringLiteral("sk-secret"));

        SettingsService reopen(&store);
        assert(reopen.apiKey() == QStringLiteral("sk-secret"));

        reopen.setApiKey(QString());
        reopen.save();
        SettingsService reopen2(&store);
        assert(reopen2.apiKey().isEmpty());
    }

    {
        FailingSecretStore store;
        SettingsService service(&store);
        service.setApiKey(QStringLiteral("sk-write-fails"));
        assert(!service.save());
        assert(service.apiKey() == QStringLiteral("sk-write-fails"));
    }

    {
        QTemporaryDir personaDir;
        assert(personaDir.isValid());
        const bool hadMilesDataDir = qEnvironmentVariableIsSet("MILES_DATA_DIR");
        const QByteArray previousMilesDataDir = qgetenv("MILES_DATA_DIR");
        qputenv("MILES_DATA_DIR", personaDir.path().toUtf8());

        InMemorySecretStore store;
        SettingsService service(&store);
        PetRuntime runtime;
        assert(runtime.activeSkinId() == QStringLiteral("miles-edgeworth"));

        SettingsController controller(&service, &runtime);
        controller.openWindow();
        const QString savedPersona = QStringLiteral("Settings saved persona\n御剑怜侍保持正式中文语气。\n");
        controller.setPersonaPrompt(savedPersona);
        controller.save();

        assert(controller.personaError().isEmpty());
        assert(runtime.manifest().personaPrompt == savedPersona);
        assert(runtime.reloadActiveSkin());
        assert(runtime.manifest().personaPrompt == savedPersona);

        if (hadMilesDataDir) {
            qputenv("MILES_DATA_DIR", previousMilesDataDir);
        } else {
            qunsetenv("MILES_DATA_DIR");
        }
    }

    {
        QTemporaryDir personaDir;
        assert(personaDir.isValid());
        const bool hadMilesDataDir = qEnvironmentVariableIsSet("MILES_DATA_DIR");
        const QByteArray previousMilesDataDir = qgetenv("MILES_DATA_DIR");
        qputenv("MILES_DATA_DIR", personaDir.path().toUtf8());

        const QString invalidSkinRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/skins/invalid-id-skin");
        QDir invalidSkinDir(invalidSkinRoot);
        assert(invalidSkinDir.mkpath(QStringLiteral("assets/body/idle")));
        assert(writeFile(invalidSkinDir.filePath(QStringLiteral("skin.json")), QStringLiteral(R"JSON(
{"id":"bad/skin","name":"非法 id 皮肤","version":"1.0.0","manifestVersion":1}
)JSON")));
        assert(writeFile(invalidSkinDir.filePath(QStringLiteral("manifest.json")), QStringLiteral(R"JSON(
{
  "states": { "idle": { "action": "idle_stand" } },
  "actions": {
    "idle_stand": {
      "variants": {
        "right": { "animation": "skin:assets/body/idle/stand.gif" }
      }
    }
  }
}
)JSON")));

        InMemorySecretStore store;
        SettingsService service(&store);
        service.setBaseUrl(QStringLiteral("https://before.example.com"));
        service.save();

        PetRuntime runtime;
        assert(runtime.setActiveSkin(QStringLiteral("bad/skin")));

        SettingsController controller(&service, &runtime);
        controller.openWindow();
        assert(controller.windowVisible());
        controller.setBaseUrl(QStringLiteral("https://after.example.com"));
        controller.setPersonaPrompt(QStringLiteral("Should fail"));
        controller.save();

        assert(controller.windowVisible());
        assert(!controller.personaError().isEmpty());
        assert(service.baseUrl() == QStringLiteral("https://before.example.com"));
        SettingsService reopened(&store);
        assert(reopened.baseUrl() == QStringLiteral("https://before.example.com"));

        if (hadMilesDataDir) {
            qputenv("MILES_DATA_DIR", previousMilesDataDir);
        } else {
            qunsetenv("MILES_DATA_DIR");
        }
    }

    return 0;
}
