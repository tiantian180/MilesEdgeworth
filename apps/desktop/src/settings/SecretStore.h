#pragma once

#include <QString>
#include <memory>

// SecretStore is a thin abstraction over the OS keychain. The desktop reads the
// API key from here and injects it into the sidecar's QProcessEnvironment.
class SecretStore
{
public:
    virtual ~SecretStore() = default;

    virtual bool available() const = 0;
    virtual QString read(const QString &service, const QString &account) = 0;
    virtual bool write(const QString &service, const QString &account, const QString &secret) = 0;
    virtual bool remove(const QString &service, const QString &account) = 0;

    static std::unique_ptr<SecretStore> create();
};

class NullSecretStore : public SecretStore
{
public:
    bool available() const override { return false; }
    QString read(const QString &, const QString &) override { return {}; }
    bool write(const QString &, const QString &, const QString &) override { return false; }
    bool remove(const QString &, const QString &) override { return true; }
};
