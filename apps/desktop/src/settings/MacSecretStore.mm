#include "MacSecretStore.h"

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

NSMutableDictionary *baseQuery(const QString &service, const QString &account, bool dataProtectionKeychain)
{
    NSMutableDictionary *query = [NSMutableDictionary dictionary];
    query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    query[(__bridge id)kSecAttrService] = toNSString(service);
    query[(__bridge id)kSecAttrAccount] = toNSString(account);
    if (dataProtectionKeychain) {
        // macOS 传统 login keychain 会把访问权限绑到当前构建的 code hash。
        // 开发期 adhoc 签名每次构建都变，容易反复弹“允许访问钥匙串”。
        // Data Protection Keychain 是 Apple 推荐给 SecItem 的现代实现，行为更接近 iOS。
        query[(__bridge id)kSecUseDataProtectionKeychain] = (__bridge id)kCFBooleanTrue;
    }
    return query;
}

QString readSecret(const QString &service, const QString &account, bool dataProtectionKeychain)
{
    NSMutableDictionary *query = baseQuery(service, account, dataProtectionKeychain);
    query[(__bridge id)kSecReturnData] = (__bridge id)kCFBooleanTrue;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess || result == nullptr) {
        return {};
    }

    NSData *data = (__bridge NSData *)result;
    const QString secret =
        QString::fromUtf8(static_cast<const char *>(data.bytes), static_cast<qsizetype>(data.length));
    CFRelease(result);
    return secret;
}

QString readDataProtectionSecret(const QString &service, const QString &account)
{
    return readSecret(service, account, true);
}

QString readLegacySecret(const QString &service, const QString &account)
{
    return readSecret(service, account, false);
}

bool removeSecret(const QString &service, const QString &account, bool dataProtectionKeychain)
{
    NSMutableDictionary *query = baseQuery(service, account, dataProtectionKeychain);
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    return status == errSecSuccess || status == errSecItemNotFound;
}
} // namespace

QString MacSecretStore::read(const QString &service, const QString &account)
{
    const QString secret = readDataProtectionSecret(service, account);
    if (!secret.isEmpty()) {
        return secret;
    }

    const QString legacySecret = readLegacySecret(service, account);
    if (!legacySecret.isEmpty()) {
        write(service, account, legacySecret);
    }
    return legacySecret;
}

bool MacSecretStore::write(const QString &service, const QString &account, const QString &secret)
{
    if (secret.isEmpty()) {
        return remove(service, account);
    }

    NSMutableDictionary *query = baseQuery(service, account, true);
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
    attrs[(__bridge id)kSecValueData] = toNSData(secret);

    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attrs);
    if (status == errSecSuccess) {
        return true;
    }
    if (status != errSecItemNotFound) {
        return false;
    }

    NSMutableDictionary *addQuery = baseQuery(service, account, true);
    addQuery[(__bridge id)kSecValueData] = toNSData(secret);
    addQuery[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    return status == errSecSuccess;
}

bool MacSecretStore::remove(const QString &service, const QString &account)
{
    const bool removedDataProtection = removeSecret(service, account, true);
    const bool removedLegacy = removeSecret(service, account, false);
    return removedDataProtection && removedLegacy;
}
