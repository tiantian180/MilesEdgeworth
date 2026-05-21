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

NSMutableDictionary *baseQuery(const QString &service, const QString &account)
{
    NSMutableDictionary *query = [NSMutableDictionary dictionary];
    query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    query[(__bridge id)kSecAttrService] = toNSString(service);
    query[(__bridge id)kSecAttrAccount] = toNSString(account);
    return query;
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
        return true;
    }
    if (status != errSecItemNotFound) {
        return false;
    }

    NSMutableDictionary *addQuery = baseQuery(service, account);
    addQuery[(__bridge id)kSecValueData] = toNSData(secret);
    status = SecItemAdd((__bridge CFDictionaryRef)addQuery, nullptr);
    return status == errSecSuccess;
}

bool MacSecretStore::remove(const QString &service, const QString &account)
{
    NSMutableDictionary *query = baseQuery(service, account);
    const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    return status == errSecSuccess || status == errSecItemNotFound;
}
