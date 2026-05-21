#include "pet/commands/SkinCommandResolver.h"

namespace {
bool commandEnabled(const SkinCommandDefinition &command, const RuntimeSnapshot &snapshot)
{
    // 第一版只支持最小 enabledWhen.notAction。
    // 这里刻意保持窄接口，避免把完整条件表达式提前塞进框架层。
    if (!command.disabledWhenActionId.isEmpty()
            && snapshot.currentActionId == command.disabledWhenActionId) {
        return false;
    }

    return true;
}
} // namespace

QList<ResolvedSkinCommand> SkinCommandResolver::enabledCommands(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot
)
{
    QList<ResolvedSkinCommand> commands;
    for (const SkinCommandDefinition &command : manifest.skinCommands) {
        if (commandEnabled(command, snapshot)) {
            commands.append({command.id, command.label});
        }
    }
    return commands;
}

ActionRequest SkinCommandResolver::resolveCommand(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const QString &commandId
)
{
    const QString normalizedCommandId = commandId.trimmed();
    if (!manifest.skinCommands.contains(normalizedCommandId)) {
        return ActionRequest::none();
    }

    const SkinCommandDefinition command = manifest.skinCommands.value(normalizedCommandId);
    if (!commandEnabled(command, snapshot)) {
        return ActionRequest::none();
    }

    return command.request;
}
