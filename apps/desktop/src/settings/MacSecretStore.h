#pragma once

#include "SecretStore.h"

class MacSecretStore : public SecretStore
{
public:
    bool available() const override { return true; }
    QString read(const QString &service, const QString &account) override;
    bool write(const QString &service, const QString &account, const QString &secret) override;
    bool remove(const QString &service, const QString &account) override;
};
