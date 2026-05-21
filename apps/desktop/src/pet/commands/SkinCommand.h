#pragma once

#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <QList>
#include <QString>
#include <QVariantMap>

// SkinCommand 是皮肤包暴露给菜单或快捷入口的简单命令。
//
// 它只负责把一个稳定 command id 映射成 ActionRequest，不承载任意脚本逻辑。
// 需要概率、额外 Prop、复杂流程的玩法后续进入 Custom Interaction 层。
// SkinCommandDefinition 是 manifest.skinCommands 里声明的单条命令。
// disabledWhenActionId 提供一个简单的"当前正在播该 action 时禁用"开关。
struct SkinCommandDefinition
{
    QString id;
    QString label;
    QString disabledWhenActionId;
    ActionRequest request;
};

// ResolvedSkinCommand 是过滤后给菜单 / QML 显示的简化版本（只保留 id 和 label）。
struct ResolvedSkinCommand
{
    QString id;
    QString label;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
        };
    }
};
