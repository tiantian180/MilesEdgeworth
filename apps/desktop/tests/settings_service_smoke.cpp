#include "settings/SecretStore.h"
#include "settings/SettingsController.h"
#include "settings/SettingsService.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

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

    int readCount() const { return m_readCount; }

private:
    std::map<std::pair<QString, QString>, QString> m_store;
    int m_readCount = 0;
};

void setupQSettingsScope(QTemporaryDir &dir)
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("tian-test"));
    QCoreApplication::setApplicationName(QStringLiteral("MilesEdgeworth-test"));
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

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
        assert(store.readCount() == 1);
        assert(controller.apiKey() == QStringLiteral("sk-secret"));

        controller.openWindow();
        assert(store.readCount() == 1);
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

    return 0;
}
