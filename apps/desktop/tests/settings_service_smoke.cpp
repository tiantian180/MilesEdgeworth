#include "settings/ProviderConfigFile.h"
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

namespace {
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

ProviderConfigFile::ModelConfig modelConfig(const QString &name,
                                            const QString &baseUrl,
                                            const QString &apiKey,
                                            const QString &model)
{
    ProviderConfigFile::ModelConfig cfg;
    cfg.name = name;
    cfg.baseUrl = baseUrl;
    cfg.apiKey = apiKey;
    cfg.model = model;
    return cfg;
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
        const QString path = tmp.filePath(QStringLiteral("settings.json"));
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
        ProviderConfigFile::LangfuseConfig langfuse;
        langfuse.enabled = true;
        langfuse.host = QStringLiteral("https://cloud.langfuse.com");
        langfuse.publicKey = QStringLiteral("pk-lf-test");
        langfuse.secretKey = QStringLiteral("sk-lf-test");
        langfuse.captureContent = false;
        assert(file.setConfig(cfg.name, cfg));
        file.setActiveModelConfig(cfg.name);
        file.setMsPerChar(60);
        file.setLangfuseConfig(langfuse);
        assert(file.save());

        QFile saved(path);
        assert(saved.open(QIODevice::ReadOnly));
        const auto savedDoc = QJsonDocument::fromJson(saved.readAll());
        assert(savedDoc.object().value(QStringLiteral("msPerChar")).isUndefined());
        assert(savedDoc.object().value(QStringLiteral("chat")).toObject().value(QStringLiteral("msPerChar")).toInt() == 60);
        const auto savedLangfuse = savedDoc.object().value(QStringLiteral("langfuse")).toObject();
        assert(savedLangfuse.value(QStringLiteral("enabled")).toBool());
        assert(savedLangfuse.value(QStringLiteral("host")).toString() == QStringLiteral("https://cloud.langfuse.com"));
        assert(savedLangfuse.value(QStringLiteral("publicKey")).toString() == QStringLiteral("pk-lf-test"));
        assert(savedLangfuse.value(QStringLiteral("secretKey")).toString() == QStringLiteral("sk-lf-test"));
        assert(!savedLangfuse.value(QStringLiteral("captureContent")).toBool());

        ProviderConfigFile reopened(path);
        assert(reopened.load() == ProviderConfigFile::LoadStatus::Ok);
        assert(reopened.activeModelConfig() == QStringLiteral("deepseek"));
        assert(reopened.config(QStringLiteral("deepseek")).apiKey == QStringLiteral("sk-test"));
        assert(reopened.config(QStringLiteral("deepseek")).temperature.has_value());
        assert(!reopened.config(QStringLiteral("deepseek")).maxTokens.has_value());
        assert(reopened.msPerChar() == 60);
        assert(reopened.langfuseConfig().enabled);
        assert(reopened.langfuseConfig().host == QStringLiteral("https://cloud.langfuse.com"));
        assert(reopened.langfuseConfig().publicKey == QStringLiteral("pk-lf-test"));
        assert(reopened.langfuseConfig().secretKey == QStringLiteral("sk-lf-test"));
        assert(!reopened.langfuseConfig().captureContent);
    }

    {
        QTemporaryDir invalidDir;
        assert(invalidDir.isValid());
        const QString path = invalidDir.filePath(QStringLiteral("settings.json"));
        assert(writeFile(path, QStringLiteral("{not-json")));
        ProviderConfigFile file(path);
        assert(file.load() == ProviderConfigFile::LoadStatus::ParseError);
        assert(!QFile::exists(path));
        assert(QFile::exists(invalidDir.filePath(QStringLiteral("settings.json.bak"))));
        assert(file.lastError().contains(QStringLiteral("已备份")));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("providers-extra.json"));
        assert(writeFile(path, QStringLiteral(R"JSON(
{
  "rootUnknown": "keep-root",
  "activeModelConfig": "deepseek",
  "chat": {
    "msPerChar": 90,
    "chatUnknown": "keep-chat"
  },
  "langfuse": {
    "enabled": true,
    "host": "https://cloud.langfuse.com",
    "publicKey": "pk-lf-test",
    "secretKey": "sk-lf-test",
    "captureContent": false,
    "langfuseUnknown": "keep-langfuse"
  },
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
        assert(file.msPerChar() == 90);
        assert(file.langfuseConfig().enabled);
        assert(file.langfuseConfig().extraFields.value(QStringLiteral("langfuseUnknown")).toString()
               == QStringLiteral("keep-langfuse"));
        auto cfg = file.config(QStringLiteral("deepseek"));
        assert(cfg.extraFields.value(QStringLiteral("providerUnknown")).toString() == QStringLiteral("keep-provider"));
        cfg.model = QStringLiteral("deepseek-reasoner");
        auto langfuse = file.langfuseConfig();
        langfuse.host = QStringLiteral("https://us.cloud.langfuse.com");
        file.setLangfuseConfig(langfuse);
        assert(file.setConfig(cfg.name, cfg));
        assert(file.save());

        QFile saved(path);
        assert(saved.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(saved.readAll());
        const auto root = doc.object();
        assert(root.value(QStringLiteral("rootUnknown")).toString() == QStringLiteral("keep-root"));
        assert(root.value(QStringLiteral("msPerChar")).isUndefined());
        const auto chat = root.value(QStringLiteral("chat")).toObject();
        assert(chat.value(QStringLiteral("msPerChar")).toInt() == 90);
        assert(chat.value(QStringLiteral("chatUnknown")).toString() == QStringLiteral("keep-chat"));
        const auto savedLangfuse = root.value(QStringLiteral("langfuse")).toObject();
        assert(savedLangfuse.value(QStringLiteral("host")).toString() == QStringLiteral("https://us.cloud.langfuse.com"));
        assert(savedLangfuse.value(QStringLiteral("langfuseUnknown")).toString() == QStringLiteral("keep-langfuse"));
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
        const QString path = tmp.filePath(QStringLiteral("settings-controller-configs.json"));
        SettingsService service(path);
        auto first = modelConfig(QStringLiteral("first"),
                                 QStringLiteral("https://first.example.com"),
                                 QStringLiteral("sk-first"),
                                 QStringLiteral("first-model"));
        auto second = modelConfig(QStringLiteral("second"),
                                  QStringLiteral("https://second.example.com"),
                                  QStringLiteral("sk-second"),
                                  QStringLiteral("second-model"));
        assert(service.setModelConfig(first.name, first));
        assert(service.setModelConfig(second.name, second));
        service.setActiveModelConfig(first.name);
        assert(service.save());

        SettingsController controller(&service);
        controller.openWindow();
        controller.selectConfig(second.name);
        controller.revert();
        assert(service.activeModelConfig() == first.name);

        controller.openWindow();
        controller.addConfig();
        controller.setConfigName(second.name);
        controller.setBaseUrl(QStringLiteral("https://new.example.com"));
        controller.setApiKey(QStringLiteral("sk-new"));
        controller.setModel(QStringLiteral("new-model"));
        controller.save();
        assert(!controller.validationError().isEmpty());

        SettingsService reopened(path);
        assert(reopened.modelConfig(second.name).baseUrl == QStringLiteral("https://second.example.com"));
        assert(reopened.modelConfig(second.name).apiKey == QStringLiteral("sk-second"));
        assert(reopened.modelConfig(second.name).model == QStringLiteral("second-model"));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("settings-controller-extra.json"));
        assert(writeFile(path, QStringLiteral(R"JSON(
{
  "activeModelConfig": "deepseek",
  "modelConfigs": [
    {
      "name": "deepseek",
      "baseUrl": "https://api.deepseek.com",
      "apiKey": "sk-test",
      "model": "deepseek-chat",
      "providerUnknown": "keep-provider"
    }
  ]
}
)JSON")));

        SettingsService service(path);
        SettingsController controller(&service);
        controller.openWindow();
        controller.setModel(QStringLiteral("deepseek-reasoner"));
        controller.save();

        SettingsService reopened(path);
        const auto cfg = reopened.modelConfig(QStringLiteral("deepseek"));
        assert(cfg.model == QStringLiteral("deepseek-reasoner"));
        assert(cfg.extraFields.value(QStringLiteral("providerUnknown")).toString()
               == QStringLiteral("keep-provider"));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("settings-controller-delete.json"));
        SettingsService service(path);
        auto first = modelConfig(QStringLiteral("first"),
                                 QStringLiteral("https://first.example.com"),
                                 QStringLiteral("sk-first"),
                                 QStringLiteral("first-model"));
        auto second = modelConfig(QStringLiteral("second"),
                                  QStringLiteral("https://second.example.com"),
                                  QStringLiteral("sk-second"),
                                  QStringLiteral("second-model"));
        assert(service.setModelConfig(first.name, first));
        assert(service.setModelConfig(second.name, second));
        service.setActiveModelConfig(first.name);
        assert(service.save());

        SettingsController controller(&service);
        int savedCount = 0;
        QObject::connect(&controller, &SettingsController::saved, [&savedCount]() {
            ++savedCount;
        });
        controller.openWindow();
        controller.deleteConfig(second.name);

        assert(savedCount == 0);
        assert(service.activeModelConfig() == first.name);
        assert(!service.configNames().contains(second.name));
    }

    {
        const QString path = tmp.filePath(QStringLiteral("settings-service.json"));
        SettingsService service(path);

        assert(service.baseUrl().isEmpty());
        assert(service.model().isEmpty());
        assert(!service.temperature().has_value());
        assert(!service.maxTokens().has_value());
        assert(service.msPerChar() == 80);
        assert(!service.langfuseConfig().enabled);
        assert(service.langfuseConfig().captureContent);
        assert(!service.providerConfigured());

        auto cfg = modelConfig(QStringLiteral("deepseek"),
                               QStringLiteral("https://api.example.com"),
                               QStringLiteral("sk-secret"),
                               QStringLiteral("gpt-test"));
        cfg.temperature = 0.3;
        cfg.maxTokens = std::nullopt;
        assert(service.setModelConfig(cfg.name, cfg));
        service.setActiveModelConfig(cfg.name);
        service.setMsPerChar(120);
        ProviderConfigFile::LangfuseConfig langfuse;
        langfuse.enabled = true;
        langfuse.host = QStringLiteral("https://cloud.langfuse.com");
        langfuse.publicKey = QStringLiteral("pk-lf-test");
        langfuse.secretKey = QStringLiteral("sk-lf-test");
        langfuse.captureContent = false;
        service.setLangfuseConfig(langfuse);
        assert(service.save());

        SettingsService reopened(path);
        assert(reopened.configNames() == QStringList({QStringLiteral("deepseek")}));
        assert(reopened.activeModelConfig() == QStringLiteral("deepseek"));
        assert(reopened.baseUrl() == QStringLiteral("https://api.example.com"));
        assert(reopened.apiKey() == QStringLiteral("sk-secret"));
        assert(reopened.model() == QStringLiteral("gpt-test"));
        assert(reopened.temperature().has_value());
        assert(qFuzzyCompare(*reopened.temperature() + 1.0, 0.3 + 1.0));
        assert(!reopened.maxTokens().has_value());
        assert(reopened.msPerChar() == 120);
        assert(reopened.langfuseConfig().enabled);
        assert(reopened.langfuseConfig().host == QStringLiteral("https://cloud.langfuse.com"));
        assert(reopened.langfuseConfig().publicKey == QStringLiteral("pk-lf-test"));
        assert(reopened.langfuseConfig().secretKey == QStringLiteral("sk-lf-test"));
        assert(!reopened.langfuseConfig().captureContent);
        assert(reopened.providerConfigured());

        SettingsController controller(&reopened);
        controller.openWindow();
        assert(controller.configName() == QStringLiteral("deepseek"));
        assert(controller.apiKey() == QStringLiteral("sk-secret"));
        assert(controller.langfuseEnabled());
        assert(controller.langfuseHost() == QStringLiteral("https://cloud.langfuse.com"));
        controller.setApiKey(QStringLiteral("sk-replaced"));
        controller.setLangfuseHost(QStringLiteral("https://us.cloud.langfuse.com"));
        controller.save();
        SettingsService replaced(path);
        assert(replaced.apiKey() == QStringLiteral("sk-replaced"));
        assert(replaced.langfuseConfig().host == QStringLiteral("https://us.cloud.langfuse.com"));
    }

    {
        QTemporaryDir personaDir;
        assert(personaDir.isValid());
        const bool hadMilesDataDir = qEnvironmentVariableIsSet("MILES_DATA_DIR");
        const QByteArray previousMilesDataDir = qgetenv("MILES_DATA_DIR");
        qputenv("MILES_DATA_DIR", personaDir.path().toUtf8());

        SettingsService service(personaDir.filePath(QStringLiteral("settings.json")));
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

        SettingsService service(personaDir.filePath(QStringLiteral("settings.json")));
        auto cfg = modelConfig(QStringLiteral("deepseek"),
                               QStringLiteral("https://before.example.com"),
                               QStringLiteral("sk-before"),
                               QStringLiteral("deepseek-chat"));
        assert(service.setModelConfig(cfg.name, cfg));
        service.setActiveModelConfig(cfg.name);
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
        SettingsService reopened(personaDir.filePath(QStringLiteral("settings.json")));
        assert(reopened.baseUrl() == QStringLiteral("https://before.example.com"));

        if (hadMilesDataDir) {
            qputenv("MILES_DATA_DIR", previousMilesDataDir);
        } else {
            qunsetenv("MILES_DATA_DIR");
        }
    }

    return 0;
}
