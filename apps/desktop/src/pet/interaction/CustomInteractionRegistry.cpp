#include "pet/interaction/CustomInteractionRegistry.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
#include <QtGlobal>

#include <exception>
#include <utility>
#include <vector>

namespace {
// 当前 v2 只有一个 PetRuntime 实例，因此 CI 注册表和 handler 状态先放在进程级容器中。
// 如果后续支持多只桌宠或多皮肤并存，这里需要下沉到每个 Runtime / SkinSession 持有，
// 否则不同实例会共享 handler 列表和 per-handler 内存状态。
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

QString normalizedInteractionId(const CustomInteraction &interaction)
{
    return interaction.id().trimmed();
}

bool hasRegisteredInteraction(const QString &interactionId)
{
    for (const std::unique_ptr<CustomInteraction> &interaction : registeredInteractions()) {
        if (interaction && normalizedInteractionId(*interaction) == interactionId) {
            return true;
        }
    }
    return false;
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
    m_result.requests.append(ActionRequest::animationPool(poolId));
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

void CustomInteractionHostApi::hideCurrentProp()
{
    m_result.requests.append(ActionRequest::none().withHiddenCurrentProp());
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

QVariantMap CustomInteractionHostApi::manifestConfig() const
{
    return m_manifest.customInteractionConfigs.value(m_handlerId);
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
    if (!interaction) {
        return;
    }

    const QString interactionId = normalizedInteractionId(*interaction);
    if (interactionId.isEmpty() || hasRegisteredInteraction(interactionId)) {
        return;
    }

    registeredInteractions().push_back(std::move(interaction));
}

void CustomInteractionRegistry::registerBuiltins(const SkinManifest &manifest)
{
    // Phase 0.70 只提供一个无副作用观察型 handler，用于验证注册和分发管线。
    // Miles 专属玩法会在后续 Phase 以独立 handler 接回，不写进这里。
    for (const QString &interactionId : manifest.customInteractions) {
        if (interactionId == QStringLiteral("debug.observe")) {
            registerInteraction(std::make_unique<ObserverCustomInteraction>());
        }
    }
}

void CustomInteractionRegistry::reset()
{
    registeredInteractions().clear();
    interactionStates().clear();
}

void CustomInteractionRegistry::clearForTest()
{
    reset();
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
            const QString interactionId = normalizedInteractionId(*interaction);
            QVariantMap &state = interactionStates()[interactionId];
            CustomInteractionHostApi host(manifest, snapshot, event, interactionId, &state);
            const CustomInteractionResult handlerResult = interaction->handleEvent(event, snapshot, host);
            const CustomInteractionResult merged = mergeResults(host.result(), handlerResult);

            combined.requests.append(merged.requests);
            combined.continueDefault = combined.continueDefault && merged.continueDefault;
            combined.stopPropagation = combined.stopPropagation || merged.stopPropagation;

            if (combined.stopPropagation) {
                break;
            }
        } catch (const std::exception &error) {
            // 高级交互失败时回退默认逻辑，避免皮肤脚本破坏桌宠主流程。
            qWarning().noquote() << "CustomInteraction handler failed:"
                                 << normalizedInteractionId(*interaction)
                                 << error.what();
            continue;
        } catch (...) {
            qWarning().noquote() << "CustomInteraction handler failed:"
                                 << normalizedInteractionId(*interaction)
                                 << "unknown exception";
            continue;
        }
    }

    return combined;
}
