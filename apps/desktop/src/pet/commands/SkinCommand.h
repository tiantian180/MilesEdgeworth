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
struct SkinCommandDefinition
{
    QString id;
    QString label;
    QString disabledWhenActionId;
    ActionRequest request;
};

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
