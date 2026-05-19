#pragma once

#include "pet/commands/SkinCommand.h"
#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QString>

class SkinCommandResolver
{
public:
    static QList<ResolvedSkinCommand> enabledCommands(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot
    );

    static ActionRequest resolveCommand(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const QString &commandId
    );
};
