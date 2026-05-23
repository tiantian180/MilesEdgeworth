#include "MacSecretStore.h"

#include "settings/SettingsLogging.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <QByteArray>

namespace {
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

NSMutableDictionary *baseQuery(const QString &service, const QString &account)
{
    NSMutableDictionary *query = [NSMutableDictionary dictionary];
    query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    query[(__bridge id)kSecAttrService] = toNSString(service);
    query[(__bridge id)kSecAttrAccount] = toNSString(account);
    // macOS 传统 login keychain 会把访问权限绑到当前构建的 code hash。
    // 开发期 adhoc 签名每次构建都变，容易反复弹“允许访问钥匙串”。
    // Data Protection Keychain 是 Apple 推荐给 SecItem 的现代实现，行为更接近 iOS。
    query[(__bridge id)kSecUseDataProtectionKeychain] = (__bridge id)kCFBooleanTrue;
    return query;
}

QString statusValue(OSStatus status)
{
    return QStringLiteral("status=%1").arg(status);
}
} // namespace

QString MacSecretStore::read(const QString &service, const QString &account)
{
    NSMutableDictionary *query = baseQuery(service, account);
    query[(__bridge id)kSecReturnData] = (__bridge id)kCFBooleanTrue;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess || result == nullptr) {
        if (status != errSecItemNotFound) {
            qCWarning(settingsLog).noquote() << "keychain read failed"
                                             << statusValue(status)
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

bool MacSecretStore::write(const QString &service, const QString &account, const QString &secret)
{
    if (secret.isEmpty()) {
        return remove(service, account);
    }

    NSMutableDictionary *query = baseQuery(service, account);
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
    attrs[(__bridge id)kSecValueData] = toNSData(secret);

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attrs);
    if (status == errSecSuccess) {
        qCDebug(settingsLog).noquote() << "keychain secret updated"
                                       << QStringLiteral("service=%1").arg(service)
                                       << QStringLiteral("account=%1").arg(account);
        return true;
    }
    if (status != errSecItemNotFound) {
        qCWarning(settingsLog).noquote() << "keychain update failed"
                                         << statusValue(status)
                                         << QStringLiteral("service=%1").arg(service)
                                         << QStringLiteral("account=%1").arg(account);
        return false;
    }

    NSMutableDictionary *addQuery = baseQuery(service, account);
    addQuery[(__bridge id)kSecValueData] = toNSData(secret);
    addQuery[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    if (status != errSecSuccess) {
        qCWarning(settingsLog).noquote() << "keychain add failed"
                                         << statusValue(status)
                                         << QStringLiteral("service=%1").arg(service)
                                         << QStringLiteral("account=%1").arg(account);
    } else {
        qCDebug(settingsLog).noquote() << "keychain secret added"
                                       << QStringLiteral("service=%1").arg(service)
                                       << QStringLiteral("account=%1").arg(account);
    }
    return status == errSecSuccess;
}

bool MacSecretStore::remove(const QString &service, const QString &account)
{
    NSMutableDictionary *query = baseQuery(service, account);
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
        qCWarning(settingsLog).noquote() << "keychain remove failed"
                                         << statusValue(status)
                                         << QStringLiteral("service=%1").arg(service)
                                         << QStringLiteral("account=%1").arg(account);
    }
    return status == errSecSuccess || status == errSecItemNotFound;
}
