#include "pet/interaction/CustomInteractionRegistry.h"

#include <QRandomGenerator>
#include <QTimer>
#include <QtGlobal>

#include <exception>
#include <utility>
#include <vector>

namespace {
std::vector<std::unique_ptr<CustomInteraction>> &registeredInteractions()
{
    static std::vector<std::unique_ptr<CustomInteraction>> interactions;
    return interactions;
}

QHash<QString, QVariantMap> &interactionStates()
{
    static QHash<QString, QVariantMap> states;
    return states;
}

class ObserverCustomInteraction final : public CustomInteraction
{
public:
    QString id() const override
    {
        return QStringLiteral("debug.observe");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {
            PetEventType::PointerSingleClick,
            PetEventType::PointerDoubleClick,
            PetEventType::MenuCommand,
            PetEventType::IdleLoopFinished,
            PetEventType::RuntimeStarted,
            PetEventType::ActionCompleted,
            PetEventType::PropClicked,
            PetEventType::PropExpired,
        };
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &
    ) override
    {
        return {};
    }
};

bool shouldDispatchTo(const CustomInteraction &interaction, const PetEvent &event)
{
    return interaction.supportedEvents().isEmpty()
        || interaction.supportedEvents().contains(event.type);
}

CustomInteractionResult mergeResults(
    const CustomInteractionResult &hostResult,
    const CustomInteractionResult &handlerResult
)
{
    CustomInteractionResult merged = hostResult;
    merged.requests.append(handlerResult.requests);
    merged.continueDefault = hostResult.continueDefault && handlerResult.continueDefault;
    merged.stopPropagation = hostResult.stopPropagation || handlerResult.stopPropagation;
    return merged;
}
} // namespace

CustomInteractionHostApi::CustomInteractionHostApi(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event,
    const QString &handlerId,
    QVariantMap *state
)
    : m_manifest(manifest)
    , m_snapshot(snapshot)
    , m_event(event)
    , m_handlerId(handlerId)
    , m_state(state)
{
}

void CustomInteractionHostApi::emitAction(const QString &actionId)
{
    m_result.requests.append(ActionRequest::action(actionId));
}

void CustomInteractionHostApi::emitRecipe(const QString &recipeId)
{
    m_result.requests.append(ActionRequest::recipe(recipeId));
}

void CustomInteractionHostApi::emitPool(const QString &poolId)
{
    m_result.requests.append(ActionRequest::actionPool(poolId));
}

void CustomInteractionHostApi::emitReturnToIdle()
{
    m_result.requests.append(ActionRequest::returnToIdle());
}

void CustomInteractionHostApi::spawnProp(const QString &propId, const QVariantMap &overrides)
{
    m_result.requests.append(ActionRequest::spawnProp(propId, overrides));
}

void CustomInteractionHostApi::playSound(const QUrl &url)
{
    m_result.requests.append(ActionRequest::playSound(url));
}

double CustomInteractionHostApi::random()
{
    if (m_event.hasRandomValue) {
        return m_event.randomValue;
    }
    return QRandomGenerator::global()->generateDouble();
}

void CustomInteractionHostApi::scheduleAfter(int delayMs, std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    // 定时回调必须回到 Qt 主事件循环；handler 自己不持有线程或事件循环。
    QTimer::singleShot(qMax(0, delayMs), std::move(callback));
}

RuntimeSnapshot CustomInteractionHostApi::snapshot() const
{
    return m_snapshot;
}

QVariantMap CustomInteractionHostApi::manifestConfig(const QString &handlerId) const
{
    const QString normalizedId = handlerId.trimmed().isEmpty() ? m_handlerId : handlerId.trimmed();
    return m_manifest.customInteractionConfigs.value(normalizedId);
}

void CustomInteractionHostApi::setState(const QString &key, const QVariant &value)
{
    if (m_state == nullptr || key.trimmed().isEmpty()) {
        return;
    }
    m_state->insert(key.trimmed(), value);
}

QVariant CustomInteractionHostApi::getState(const QString &key) const
{
    if (m_state == nullptr) {
        return {};
    }
    return m_state->value(key.trimmed());
}

bool CustomInteractionHostApi::hasState(const QString &key) const
{
    return m_state != nullptr && m_state->contains(key.trimmed());
}

void CustomInteractionHostApi::skipDefault()
{
    m_result.continueDefault = false;
}

void CustomInteractionHostApi::stopPropagation()
{
    m_result.stopPropagation = true;
}

CustomInteractionResult CustomInteractionHostApi::result() const
{
    return m_result;
}

void CustomInteractionRegistry::registerInteraction(std::unique_ptr<CustomInteraction> interaction)
{
    if (!interaction || interaction->id().trimmed().isEmpty()) {
        return;
    }

    registeredInteractions().push_back(std::move(interaction));
}

void CustomInteractionRegistry::registerBuiltins(const SkinManifest &manifest)
{
    clearForTest();

    // Phase 0.70 只提供一个无副作用观察型 handler，用于验证注册和分发管线。
    // Miles 专属玩法会在后续 Phase 以独立 handler 接回，不写进这里。
    for (const QString &interactionId : manifest.customInteractions) {
        if (interactionId == QStringLiteral("debug.observe")) {
            registerInteraction(std::make_unique<ObserverCustomInteraction>());
        }
    }
}

void CustomInteractionRegistry::clearForTest()
{
    registeredInteractions().clear();
    interactionStates().clear();
}

CustomInteractionResult CustomInteractionRegistry::handleEvent(
    const SkinManifest &manifest,
    const RuntimeSnapshot &snapshot,
    const PetEvent &event
)
{
    CustomInteractionResult combined;

    for (const std::unique_ptr<CustomInteraction> &interaction : registeredInteractions()) {
        if (!interaction || !shouldDispatchTo(*interaction, event)) {
            continue;
        }

        try {
            QVariantMap &state = interactionStates()[interaction->id()];
            CustomInteractionHostApi host(manifest, snapshot, event, interaction->id(), &state);
            const CustomInteractionResult handlerResult = interaction->handleEvent(event, snapshot, host);
            const CustomInteractionResult merged = mergeResults(host.result(), handlerResult);

            combined.requests.append(merged.requests);
            combined.continueDefault = combined.continueDefault && merged.continueDefault;
            combined.stopPropagation = combined.stopPropagation || merged.stopPropagation;

            if (combined.stopPropagation) {
                break;
            }
        } catch (const std::exception &) {
            // 高级交互失败时回退默认逻辑，避免皮肤脚本破坏桌宠主流程。
            continue;
        } catch (...) {
            continue;
        }
    }

    return combined;
}
