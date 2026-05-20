#pragma once

#include "pet/manifest/SkinManifest.h"
#include "pet/requests/ActionRequest.h"

#include <QString>

struct ExpressionMappingContext
{
    QString state;
    double randomValue = 0.0;
};

// ExpressionMappingResolver 把“想表达什么”转换成最终播放请求。
//
// 模型或未来 Go sidecar 只需要提交 expression tag；具体播哪个 action /
// recipe / pool 由当前皮肤 manifest 决定。这里保持纯选择逻辑，不读取窗口、
// 不改 Runtime 状态，也不直接播放动画。
class ExpressionMappingResolver
{
public:
    static ActionRequest resolve(
        const SkinManifest &manifest,
        const ExpressionMappingContext &context,
        const QString &expressionId
    );
};
