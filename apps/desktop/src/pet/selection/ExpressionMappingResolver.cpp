#include "pet/selection/ExpressionMappingResolver.h"

#include <QSet>
#include <QtGlobal>

namespace {
constexpr auto kNeutralExpressionId = "neutral";
constexpr auto kFirstAvailableSelection = "first_available";
constexpr auto kWeightedRandomSelection = "weighted_random";

bool stateAllowed(const QStringList &allowedStates, const QString &state)
{
    return allowedStates.isEmpty() || allowedStates.contains(state);
}

bool requestExists(const SkinManifest &manifest, const ActionRequest &request)
{
    switch (request.kind) {
    case ActionRequestKind::None:
        return false;
    case ActionRequestKind::ActionPool:
        return manifest.actionPools.contains(request.targetId);
    case ActionRequestKind::Recipe:
        return manifest.recipes.contains(request.targetId);
    case ActionRequestKind::Action:
        return manifest.actions.contains(request.targetId);
    case ActionRequestKind::ReturnToIdle:
    case ActionRequestKind::ToggleFacing:
    case ActionRequestKind::SpawnProp:
    case ActionRequestKind::PlaySound:
        return true;
    }

    return false;
}

QString normalizedExpressionId(
    const SkinManifest &manifest,
    const ExpressionMappingContext &context,
    const QString &expressionId
)
{
    const QString requestedExpressionId = expressionId.trimmed();
    const ExpressionDefinition expression = manifest.expressions.value(requestedExpressionId);

    if (!requestedExpressionId.isEmpty()
        && manifest.expressions.contains(requestedExpressionId)
        && stateAllowed(expression.allowedStates, context.state)) {
        return requestedExpressionId;
    }

    return QString::fromUtf8(kNeutralExpressionId);
}

QList<ExpressionMappingEntry> availableEntries(
    const SkinManifest &manifest,
    const ExpressionMappingContext &context,
    const ExpressionMappingDefinition &mapping
)
{
    QList<ExpressionMappingEntry> entries;
    for (const ExpressionMappingEntry &entry : mapping.actions) {
        if (!stateAllowed(entry.allowedStates, context.state)) {
            continue;
        }
        if (!requestExists(manifest, entry.request)) {
            continue;
        }
        entries.append(entry);
    }
    return entries;
}

ActionRequest selectEntry(
    const ExpressionMappingContext &context,
    const ExpressionMappingDefinition &mapping,
    const QList<ExpressionMappingEntry> &entries
)
{
    if (entries.isEmpty()) {
        return ActionRequest::none();
    }

    if (mapping.selection == QString::fromUtf8(kFirstAvailableSelection)
        || mapping.selection.trimmed().isEmpty()) {
        return entries.constFirst().request;
    }

    if (mapping.selection != QString::fromUtf8(kWeightedRandomSelection)) {
        return entries.constFirst().request;
    }

    int totalWeight = 0;
    for (const ExpressionMappingEntry &entry : entries) {
        totalWeight += qMax(0, entry.weight);
    }
    if (totalWeight <= 0) {
        return entries.constFirst().request;
    }

    const double boundedRandom = qBound(0.0, context.randomValue, 0.999999);
    const double threshold = boundedRandom * totalWeight;
    int accumulatedWeight = 0;
    for (const ExpressionMappingEntry &entry : entries) {
        accumulatedWeight += qMax(0, entry.weight);
        if (threshold < accumulatedWeight) {
            return entry.request;
        }
    }

    return entries.constLast().request;
}

ActionRequest resolveInternal(
    const SkinManifest &manifest,
    const ExpressionMappingContext &context,
    const QString &expressionId,
    QSet<QString> &visitedExpressionIds
)
{
    const QString resolvedExpressionId = normalizedExpressionId(manifest, context, expressionId);
    if (visitedExpressionIds.contains(resolvedExpressionId)) {
        return ActionRequest::none();
    }
    visitedExpressionIds.insert(resolvedExpressionId);

    const ExpressionMappingDefinition mapping = manifest.expressionMappings.value(resolvedExpressionId);
    const QList<ExpressionMappingEntry> entries = availableEntries(manifest, context, mapping);
    const ActionRequest selectedRequest = selectEntry(context, mapping, entries);
    if (selectedRequest.kind != ActionRequestKind::None) {
        return selectedRequest;
    }

    const QString fallbackExpressionId = mapping.fallbackExpressionId.trimmed().isEmpty()
        ? QString::fromUtf8(kNeutralExpressionId)
        : mapping.fallbackExpressionId.trimmed();
    if (fallbackExpressionId == resolvedExpressionId) {
        return ActionRequest::none();
    }

    return resolveInternal(manifest, context, fallbackExpressionId, visitedExpressionIds);
}
} // namespace

ActionRequest ExpressionMappingResolver::resolve(
    const SkinManifest &manifest,
    const ExpressionMappingContext &context,
    const QString &expressionId
)
{
    QSet<QString> visitedExpressionIds;
    return resolveInternal(manifest, context, expressionId, visitedExpressionIds);
}
