#include "MacSecretStore.h"

#include "settings/SettingsLogging.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <QByteArray>

namespace {
enum class KeychainBackend {
    DataProtection,
    LoginFallback,
};

NSData *toNSData(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return [NSData dataWithBytes:bytes.constData() length:static_cast<NSUInteger>(bytes.size())];
}

NSString *toNSString(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return [[NSString alloc] initWithBytes:bytes.constData()
                                    length:static_cast<NSUInteger>(bytes.size())
                                  encoding:NSUTF8StringEncoding];
}

QString fallbackService(const QString &service)
{
    return service + QStringLiteral(".debug-fallback");
}

const char *backendName(KeychainBackend backend)
{
    switch (backend) {
    case KeychainBackend::DataProtection:
        return "data-protection";
    case KeychainBackend::LoginFallback:
        return "login-fallback";
    }
    return "unknown";
}

NSMutableDictionary *baseQuery(const QString &service, const QString &account, KeychainBackend backend)
{
    NSMutableDictionary *query = [NSMutableDictionary dictionary];
    query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    query[(__bridge id)kSecAttrService] = toNSString(service);
    query[(__bridge id)kSecAttrAccount] = toNSString(account);
    if (backend == KeychainBackend::DataProtection) {
        // macOS 传统 login keychain 会把访问权限绑到当前构建的 code hash。
        // 开发期 adhoc 签名每次构建都变，容易反复弹“允许访问钥匙串”。
        // Data Protection Keychain 是 Apple 推荐给 SecItem 的现代实现，行为更接近 iOS。
        query[(__bridge id)kSecUseDataProtectionKeychain] = (__bridge id)kCFBooleanTrue;
    }
    return query;
}

QString statusValue(OSStatus status)
{
    return QStringLiteral("status=%1").arg(status);
}

bool shouldUseDebugFallback(OSStatus status)
{
    return status == errSecMissingEntitlement;
}

QString readFromBackend(const QString &service,
                        const QString &account,
                        KeychainBackend backend,
                        OSStatus *finalStatus)
{
    NSMutableDictionary *query = baseQuery(service, account, backend);
    query[(__bridge id)kSecReturnData] = (__bridge id)kCFBooleanTrue;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (finalStatus != nullptr) {
        *finalStatus = status;
    }
    if (status != errSecSuccess || result == nullptr) {
        if (status != errSecItemNotFound && !shouldUseDebugFallback(status)) {
            qCWarning(settingsLog).noquote() << "keychain read failed"
                                             << statusValue(status)
                                             << QStringLiteral("backend=%1").arg(backendName(backend))
                                             << QStringLiteral("service=%1").arg(service)
                                             << QStringLiteral("account=%1").arg(account);
        }
        return {};
    }

    NSData *data = (__bridge NSData *)result;
    const QString secret =
        QString::fromUtf8(static_cast<const char *>(data.bytes), static_cast<qsizetype>(data.length));
    CFRelease(result);
    return secret;
}

bool writeToBackend(const QString &service,
                    const QString &account,
                    const QString &secret,
                    KeychainBackend backend,
                    OSStatus *finalStatus)
{
    NSMutableDictionary *query = baseQuery(service, account, backend);
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
    attrs[(__bridge id)kSecValueData] = toNSData(secret);

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attrs);
    if (status == errSecSuccess) {
        qCDebug(settingsLog).noquote() << "keychain secret updated"
                                       << QStringLiteral("backend=%1").arg(backendName(backend))
                                       << QStringLiteral("service=%1").arg(service)
                                       << QStringLiteral("account=%1").arg(account);
        if (finalStatus != nullptr) {
            *finalStatus = status;
        }
        return true;
    }
    if (status != errSecItemNotFound) {
        if (!shouldUseDebugFallback(status)) {
            qCWarning(settingsLog).noquote() << "keychain update failed"
                                             << statusValue(status)
                                             << QStringLiteral("backend=%1").arg(backendName(backend))
                                             << QStringLiteral("service=%1").arg(service)
                                             << QStringLiteral("account=%1").arg(account);
        }
        if (finalStatus != nullptr) {
            *finalStatus = status;
        }
        return false;
    }

    NSMutableDictionary *addQuery = baseQuery(service, account, backend);
    addQuery[(__bridge id)kSecValueData] = toNSData(secret);
    if (backend == KeychainBackend::DataProtection) {
        addQuery[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    }
    status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    if (finalStatus != nullptr) {
        *finalStatus = status;
    }
    if (status != errSecSuccess) {
        if (!shouldUseDebugFallback(status)) {
            qCWarning(settingsLog).noquote() << "keychain add failed"
                                             << statusValue(status)
                                             << QStringLiteral("backend=%1").arg(backendName(backend))
                                             << QStringLiteral("service=%1").arg(service)
                                             << QStringLiteral("account=%1").arg(account);
        }
        return false;
    }

    qCDebug(settingsLog).noquote() << "keychain secret added"
                                   << QStringLiteral("backend=%1").arg(backendName(backend))
                                   << QStringLiteral("service=%1").arg(service)
                                   << QStringLiteral("account=%1").arg(account);
    return true;
}

bool removeFromBackend(const QString &service, const QString &account, KeychainBackend backend, OSStatus *finalStatus)
{
    NSMutableDictionary *query = baseQuery(service, account, backend);
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (finalStatus != nullptr) {
        *finalStatus = status;
    }
    if (status != errSecSuccess && status != errSecItemNotFound) {
        if (!shouldUseDebugFallback(status)) {
            qCWarning(settingsLog).noquote() << "keychain remove failed"
                                             << statusValue(status)
                                             << QStringLiteral("backend=%1").arg(backendName(backend))
                                             << QStringLiteral("service=%1").arg(service)
                                             << QStringLiteral("account=%1").arg(account);
        }
        return false;
    }
    return true;
}
} // namespace

QString MacSecretStore::read(const QString &service, const QString &account)
{
    OSStatus status = errSecSuccess;
    const QString secret = readFromBackend(service, account, KeychainBackend::DataProtection, &status);
    if (status == errSecSuccess) {
        return secret;
    }
    if (!shouldUseDebugFallback(status)) {
        return {};
    }

    qCWarning(settingsLog).noquote()
        << "data protection keychain unavailable, reading debug fallback"
        << statusValue(status)
        << QStringLiteral("service=%1").arg(service)
        << QStringLiteral("fallbackService=%1").arg(fallbackService(service))
        << QStringLiteral("account=%1").arg(account);
    return readFromBackend(fallbackService(service), account, KeychainBackend::LoginFallback, nullptr);
}

bool MacSecretStore::write(const QString &service, const QString &account, const QString &secret)
{
    if (secret.isEmpty()) {
        return remove(service, account);
    }

    OSStatus status = errSecSuccess;
    if (writeToBackend(service, account, secret, KeychainBackend::DataProtection, &status)) {
        return true;
    }
    if (!shouldUseDebugFallback(status)) {
        return false;
    }

    qCWarning(settingsLog).noquote()
        << "data protection keychain unavailable, writing debug fallback"
        << statusValue(status)
        << QStringLiteral("service=%1").arg(service)
        << QStringLiteral("fallbackService=%1").arg(fallbackService(service))
        << QStringLiteral("account=%1").arg(account);
    return writeToBackend(fallbackService(service), account, secret, KeychainBackend::LoginFallback, nullptr);
}

bool MacSecretStore::remove(const QString &service, const QString &account)
{
    OSStatus status = errSecSuccess;
    if (removeFromBackend(service, account, KeychainBackend::DataProtection, &status)) {
        return true;
    }
    if (!shouldUseDebugFallback(status)) {
        return false;
    }

    qCWarning(settingsLog).noquote()
        << "data protection keychain unavailable, removing debug fallback"
        << statusValue(status)
        << QStringLiteral("service=%1").arg(service)
        << QStringLiteral("fallbackService=%1").arg(fallbackService(service))
        << QStringLiteral("account=%1").arg(account);
    return removeFromBackend(fallbackService(service), account, KeychainBackend::LoginFallback, nullptr);
}
