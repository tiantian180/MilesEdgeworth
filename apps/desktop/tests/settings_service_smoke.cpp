#include "settings/SecretStore.h"
#include "settings/SettingsController.h"
#include "settings/SettingsService.h"
#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
