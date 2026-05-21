#pragma once

#include "pet/commands/SkinCommand.h"
#include "pet/manifest/SkinManifest.h"

#include <QList>
#include <QString>

// SkinCommandResolver 把皮肤声明的 skinCommands 在运行时过滤 / 翻译成可用列表与 ActionRequest。
//
// 菜单或快捷入口先调用 enabledCommands 得到当前可点选项（按 disabledWhenActionId 等条件过滤），
// 用户点选后 InteractionPipeline 拿到 MenuCommand 事件，再通过 resolveCommand 翻译成 ActionRequest。
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
