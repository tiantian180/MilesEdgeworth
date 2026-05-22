#pragma once

#include "pet/manifest/SkinDescriptor.h"
#include "pet/manifest/SkinManifest.h"

#include <QString>

class PersonaStore
{
public:
    static QString dataDir();
    static QString overridePathForSkin(const QString &skinId);
    static QString readForDescriptor(const SkinDescriptor &descriptor);
    static bool writeForManifest(const SkinManifest &manifest, const QString &content, QString *errorMessage);
};
