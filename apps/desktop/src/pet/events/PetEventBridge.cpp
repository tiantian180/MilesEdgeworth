#include "pet/events/PetEventBridge.h"

#include "pet/PetRuntime.h"
#include "pet/interaction/InteractionPipeline.h"

#include <QRandomGenerator>

namespace {
constexpr auto kFeedTeaCommandId = "miles.feedTea";
}

PetEventBridge::PetEventBridge(PetRuntime *runtime, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
{
    Q_ASSERT(m_runtime != nullptr);

    // 皮肤命令可用性目前只受当前动作和 sleep phase 影响。
    // 后续菜单能力迁入 manifest 后，这个信号可以由 MenuController 统一发出。
    connect(m_runtime, &PetRuntime::currentActionChanged, this, &PetEventBridge::skinCommandAvailabilityChanged);
    connect(m_runtime, &PetRuntime::currentPhaseChanged, this, &PetEventBridge::skinCommandAvailabilityChanged);
}

QStringList PetEventBridge::enabledSkinCommandIds() const
{
    if (m_runtime == nullptr) {
        return {};
    }

    const RuntimeSnapshot snapshot = m_runtime->snapshot();
    if (snapshot.currentActionId == "sleep") {
        return {};
    }

    return {QString::fromUtf8(kFeedTeaCommandId)};
}

void PetEventBridge::submitPrimaryClick(double x, double y, double width, double height)
{
    submitEvent(PetEvent::pointerSingleClick(x, y, width, height));
}

void PetEventBridge::submitDoubleClick()
{
    submitEvent(PetEvent::pointerDoubleClick());
}

void PetEventBridge::submitMenuCommand(const QString &commandId)
{
    submitEvent(PetEvent::menuCommand(commandId));
}

void PetEventBridge::submitIdleLoopFinished()
{
    submitIdleLoopFinishedForTest(QRandomGenerator::global()->generateDouble());
}

void PetEventBridge::submitPropClicked()
{
    const RuntimeSnapshot snapshot = m_runtime->snapshot();
    submitEvent(PetEvent::propClicked(snapshot.currentPropId));
}

void PetEventBridge::submitPropExpired()
{
    const RuntimeSnapshot snapshot = m_runtime->snapshot();
    submitEvent(PetEvent::propExpired(snapshot.currentPropId));
}

void PetEventBridge::submitIdleLoopFinishedForTest(double randomValue)
{
    submitEvent(PetEvent::idleLoopFinished(randomValue));
}

void PetEventBridge::submitEvent(const PetEvent &event)
{
    if (m_runtime == nullptr) {
        return;
    }

    const RuntimeSnapshot snapshot = m_runtime->snapshot();
    const QList<ActionRequest> requests = InteractionPipeline::handleEvent(m_runtime->manifest(), snapshot, event);
    for (const ActionRequest &request : requests) {
        m_runtime->submitActionRequest(request);
    }
}
