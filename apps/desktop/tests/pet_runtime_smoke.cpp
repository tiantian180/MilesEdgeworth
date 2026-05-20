#include "pet/PetRuntime.h"
#include "pet/effects/AudioController.h"
#include "pet/events/PetEventBridge.h"
#include "pet/interaction/CustomInteractionRegistry.h"
#include "pet/requests/ActionRequest.h"
#include "skins/miles-edgeworth/MilesEdgeworthInteractions.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void require(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    std::cerr << message << '\n';
    std::exit(1);
}

void requireActionIn(const QString &actual, const QStringList &expected, const char *message)
{
    if (expected.contains(actual)) {
        return;
    }

    std::cerr << message << ": " << actual.toStdString() << '\n';
    std::exit(1);
}

bool hasSkinCommand(const QVariantList &commands, const QString &commandId)
{
    for (const QVariant &value : commands) {
        if (value.toMap().value("id").toString() == commandId) {
            return true;
        }
    }
    return false;
}

struct MovementCase
{
    const char *direction;
    int dxSign;
    int dySign;
    const char *facing;
    bool preserveFacing = false;
};

struct RecipeActionCase
{
    const char *recipeId;
    const char *actionId;
};

class ObserverCustomInteraction final : public CustomInteraction
{
public:
    explicit ObserverCustomInteraction(int *seenEvents)
        : m_seenEvents(seenEvents)
    {
    }

    QString id() const override
    {
        return QStringLiteral("test.observer");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {PetEventType::PointerSingleClick, PetEventType::PointerDoubleClick};
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &
    ) override
    {
        if (m_seenEvents != nullptr) {
            ++(*m_seenEvents);
        }
        return {};
    }

private:
    int *m_seenEvents = nullptr;
};

class SkipDefaultCustomInteraction final : public CustomInteraction
{
public:
    QString id() const override
    {
        return QStringLiteral("test.skipDefault");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {PetEventType::PointerDoubleClick};
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &host
    ) override
    {
        // Host API 只生成 ActionRequest，不直接碰 PetRuntime。
        host.emitAction(QStringLiteral("bow"));
        host.skipDefault();
        host.stopPropagation();
        return {};
    }
};

class StatefulCustomInteraction final : public CustomInteraction
{
public:
    QString id() const override
    {
        return QStringLiteral("test.stateful");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {PetEventType::PointerDoubleClick};
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &host
    ) override
    {
        const int count = host.getState(QStringLiteral("count")).toInt() + 1;
        host.setState(QStringLiteral("count"), count);
        host.emitAction(count == 1 ? QStringLiteral("bow") : QStringLiteral("idle_thinking_once"));
        host.skipDefault();
        host.stopPropagation();
        return {};
    }
};

class ScheduledCustomInteraction final : public CustomInteraction
{
public:
    explicit ScheduledCustomInteraction(int *callbackCount)
        : m_callbackCount(callbackCount)
    {
    }

    QString id() const override
    {
        return QStringLiteral("test.scheduled");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {PetEventType::PointerDoubleClick};
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &host
    ) override
    {
        int *callbackCount = m_callbackCount;
        host.scheduleAfter(10, [callbackCount]() {
            if (callbackCount != nullptr) {
                ++(*callbackCount);
            }
        });
        host.emitAction(QStringLiteral("bow"));
        host.skipDefault();
        host.stopPropagation();
        return {};
    }

private:
    int *m_callbackCount = nullptr;
};

class ThrowingCustomInteraction final : public CustomInteraction
{
public:
    QString id() const override
    {
        return QStringLiteral("test.throwing");
    }

    QSet<PetEventType> supportedEvents() const override
    {
        return {PetEventType::PointerSingleClick};
    }

    CustomInteractionResult handleEvent(
        const PetEvent &,
        const RuntimeSnapshot &,
        CustomInteractionHostApi &
    ) override
    {
        throw std::runtime_error("intentional smoke failure");
    }
};

void requireSignedDelta(double value, int expectedSign, const QString &recipeId, const char *axis)
{
    if (expectedSign > 0 && value > 0) {
        return;
    }

    if (expectedSign < 0 && value < 0) {
        return;
    }

    if (expectedSign == 0 && value == 0.0) {
        return;
    }

    std::cerr << recipeId.toStdString() << ' ' << axis << " 移动方向不符合预期: " << value << '\n';
    std::exit(1);
}

void requireNear(double actual, double expected, const char *message)
{
    if (std::abs(actual - expected) < 0.001) {
        return;
    }

    std::cerr << message << ": " << actual << " != " << expected << '\n';
    std::exit(1);
}

void requireRecipeAction(PetRuntime &runtime, const RecipeActionCase &recipeCase, const char *message)
{
    runtime.returnToIdle();
    runtime.playRecipe(QString::fromUtf8(recipeCase.recipeId));
    require(runtime.currentActionId() == QString::fromUtf8(recipeCase.actionId), message);
}

void waitForMilliseconds(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    AudioDefinition defaultOnlyAudio;
    defaultOnlyAudio.defaultVoiceLanguage = "jp";
    AudioController defaultOnlyAudioController;
    defaultOnlyAudioController.setAudioDefinition(defaultOnlyAudio);
    RecipeDefinition defaultOnlyRecipe;
    defaultOnlyRecipe.soundUrls.insert("jp", QUrl("qrc:/audio/holdit0.wav"));
    require(defaultOnlyAudioController.availableLanguages().isEmpty(), "未声明 voiceLanguages 时不应生成语言菜单数据");
    require(defaultOnlyAudioController.currentLanguageId() == "jp", "只声明 defaultVoiceLanguage 时仍应按默认语言选择声音");
    require(defaultOnlyAudioController.soundUrlForRecipe(defaultOnlyRecipe).toString() == "qrc:/audio/holdit0.wav", "默认语言应能选择对应 soundUrls");

    PetRuntime runtime;
    PetEventBridge bridge(&runtime);

    // 启动时应进入旧版公文包入场序列，而不是直接静止站立。
    require(runtime.currentRecipeId() == "startup.briefcase", "启动时应播放 startup.briefcase recipe");
    require(runtime.currentActionId() == "briefcase_in", "启动第一步应是 briefcase_in");
    require(!runtime.pointerInteractionEnabled(), "briefcase_in 期间应禁用鼠标交互");

    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(runtime.currentActionId() == "briefcase_in", "启动入场期间单击不应打断 briefcase_in");

    bridge.submitDoubleClick();
    require(runtime.currentActionId() == "briefcase_in", "启动入场期间双击不应打断 briefcase_in");

    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "briefcase_stop", "公文包入场结束后应进入 briefcase_stop");
    require(runtime.pointerInteractionEnabled(), "briefcase_in 结束后应恢复鼠标交互");

    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "公文包停下后应进入 idle_stand");
    require(runtime.currentRecipeId().isEmpty(), "启动序列结束后应清空 currentRecipeId");

    CustomInteractionRegistry::clearForTest();
    int observerEvents = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&observerEvents));
    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(observerEvents == 1, "观察型 CI 应收到单击事件");
    require(runtime.currentActionId() == "scared", "观察型 CI 不应改变默认单击行为");

    CustomInteractionRegistry::clearForTest();
    CustomInteractionRegistry::registerInteraction(std::make_unique<SkipDefaultCustomInteraction>());
    runtime.returnToIdle();
    bridge.submitDoubleClick();
    require(runtime.currentActionId() == "bow", "skipDefault CI 应能跳过默认双击并提交自己的动作请求");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    CustomInteractionRegistry::registerInteraction(std::make_unique<StatefulCustomInteraction>());
    bridge.submitDoubleClick();
    require(runtime.currentActionId() == "bow", "CI 第一次事件应能写入 per-handler 状态");
    bridge.submitDoubleClick();
    require(runtime.currentActionId() == "idle_thinking_once", "CI 第二次事件应能读到上一次写入的状态");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    int scheduledCallbacks = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<ScheduledCustomInteraction>(&scheduledCallbacks));
    bridge.submitDoubleClick();
    waitForMilliseconds(30);
    require(scheduledCallbacks == 1, "CI scheduleAfter 回调应回到 Qt 事件循环执行");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    int propagatedEvents = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<SkipDefaultCustomInteraction>());
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&propagatedEvents));
    bridge.submitDoubleClick();
    require(runtime.currentActionId() == "bow", "stopPropagation CI 应提交自己的动作");
    require(propagatedEvents == 0, "stopPropagation CI 应阻止后续 handler 处理同一事件");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    int observerAfterException = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<ThrowingCustomInteraction>());
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&observerAfterException));
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(observerAfterException == 1, "抛异常的 CI 不应阻断后续 handler");
    require(runtime.currentActionId() == "scared", "抛异常的 CI 不应阻断默认单击行为");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    int preservedEvents = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&preservedEvents));
    CustomInteractionRegistry::registerBuiltins(runtime.manifest());
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(preservedEvents == 1, "registerBuiltins 不应清空已注册 handler");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    int duplicateEvents = 0;
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&duplicateEvents));
    CustomInteractionRegistry::registerInteraction(std::make_unique<ObserverCustomInteraction>(&duplicateEvents));
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(duplicateEvents == 1, "重复 id 的 CI 不应重复分发");
    CustomInteractionRegistry::clearForTest();
    runtime.returnToIdle();

    registerMilesEdgeworthInteractions(runtime.manifest());
    runtime.setFacing("right");
    bridge.submitDoubleClickForTest(0.0);
    require(runtime.currentActionId() == "objecting", "确定性随机命中时应由徽章 CI 播放 objecting");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/takethat0.wav", "确定性随机命中时应播放看招语音");
    waitForMilliseconds(750);
    require(runtime.currentPropId() == "prosecutor_badge", "确定性随机命中后应飞出检察官徽章");
    bridge.submitPropClicked();
    require(!runtime.currentPropVisible(), "点击徽章后应隐藏 Prop");
    require(runtime.currentActionId() == "bow", "点击徽章后应触发鞠躬");
    runtime.handleAnimationFinished();

    runtime.returnToIdle();
    bridge.submitDoubleClickForTest(0.0);
    waitForMilliseconds(750);
    require(runtime.currentPropId() == "prosecutor_badge", "第二次确定性随机命中后仍应飞出检察官徽章");
    bridge.submitPropExpired();
    require(!runtime.currentPropVisible(), "徽章自然消失后应隐藏 Prop");
    require(runtime.currentActionId() == "pickup_badge", "徽章自然消失后应触发捡徽章");
    runtime.handleAnimationFinished();

    runtime.returnToIdle();
    bridge.submitDoubleClickForTest(0.99);
    waitForMilliseconds(750);
    require(!runtime.currentPropVisible(), "确定性随机落空时不应飞出检察官徽章");
    require(runtime.currentRecipeId() != "doubleClick.takeThat", "确定性随机落空时应回落到默认双击池");
    runtime.returnToIdle();

    runtime.setAudioLanguage("zh");
    bridge.submitDoubleClickForTest(0.0);
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/takethat2.wav", "徽章 CI 应沿用当前语音语言选择看招音频");
    waitForMilliseconds(750);
    bridge.submitPropExpired();
    runtime.handleAnimationFinished();
    runtime.setAudioLanguage("jp");
    runtime.returnToIdle();

    require(runtime.petSizeId() == "medium", "默认尺寸档位应为中");
    requireNear(runtime.petScale(), 2.0, "默认 scale 应对应旧版中号 scale=2");
    requireNear(runtime.petWindowSize(), 240.0, "默认窗口尺寸应对应旧版中号 scale=2");
    requireNear(runtime.petImageSize(), 200.0, "默认动画尺寸应对应旧版中号 scale=2");

    runtime.setPetSize("mini");
    require(runtime.petSizeId() == "mini", "setPetSize(mini) 应切到迷你档");
    requireNear(runtime.petScale(), 1.0, "迷你档应对应旧版 scale=1");
    requireNear(runtime.petWindowSize(), 120.0, "迷你档窗口尺寸应随 scale 缩小");
    runtime.playRecipe("walk.east");
    requireNear(runtime.consumeFrameMovementDelta().value("dx").toDouble(), 4.2, "mini walk.east 应按旧版 scale=1 移动");

    runtime.setPetSize("small");
    require(runtime.petSizeId() == "small", "setPetSize(small) 应切到小档");
    requireNear(runtime.petScale(), 1.5, "小档应对应旧版 scale=1.5");
    requireNear(runtime.petWindowSize(), 180.0, "小档窗口尺寸应随 scale 缩放");

    runtime.setPetSize("big");
    require(runtime.petSizeId() == "big", "setPetSize(big) 应切到大档");
    requireNear(runtime.petScale(), 3.0, "大档应对应旧版 scale=3");
    requireNear(runtime.petWindowSize(), 360.0, "大档窗口尺寸应随 scale 放大");
    runtime.playRecipe("walk.east");
    requireNear(runtime.consumeFrameMovementDelta().value("dx").toDouble(), 12.6, "big walk.east 应按旧版 scale=3 移动");

    runtime.setPetSize("medium");
    require(runtime.petSizeId() == "medium", "setPetSize(medium) 应切回中档");
    runtime.returnToIdle();

    bridge.submitIdleLoopFinishedForTest(0.71);
    require(runtime.currentActionId() == "idle_stand", "站立循环随机数超过 0.7 时应继续站立");
    require(runtime.currentRecipeId().isEmpty(), "站立循环随机数超过 0.7 时不应进入随机 recipe");

    const QString facingBeforeRandomIdle = runtime.currentFacing();
    bridge.submitIdleLoopFinishedForTest(0.69);
    require(
        !runtime.currentRecipeId().isEmpty() || runtime.currentFacing() != facingBeforeRandomIdle,
        "站立循环随机数不超过 0.7 时应进入随机 idle 行为"
    );
    runtime.returnToIdle();

    runtime.playRecipe("doubleClick.holdIt");
    bridge.submitIdleLoopFinishedForTest(0.0);
    require(runtime.currentRecipeId() == "doubleClick.holdIt", "非待机 recipe 播放中不应被站立循环入口打断");
    runtime.returnToIdle();

    // 前面的随机 idle 可能抽到转身或移动动作；这里固定朝向，
    // 让下面的转身检查只验证 turn.once 本身。
    runtime.setFacing("right");
    runtime.playRecipe("turn.once");
    require(runtime.currentActionId() == "turn_around", "turn.once 应播放 turn_around");
    require(runtime.currentFacing() == "right", "默认朝向应为 right");
    runtime.handleAnimationFinished();
    require(runtime.currentFacing() == "left", "转身播完后应切到 left");
    require(runtime.currentActionId() == "idle_stand", "转身播完后应回到 idle_stand");

    runtime.setFacing("right");
    runtime.playRecipe("idle.flipStand");
    require(runtime.currentActionId() == "idle_stand", "idle.flipStand 应保持站立动作");
    require(runtime.currentFacing() == "left", "idle.flipStand 应从 right 直接切到 left");

    runtime.playRecipe("idle.flipStand");
    require(runtime.currentActionId() == "idle_stand", "idle.flipStand 应保持站立动作");
    require(runtime.currentFacing() == "right", "idle.flipStand 应从 left 直接切到 right");

    const RecipeActionCase idleRecipeCases[] = {
        {"idle.randomThinking", "idle_thinking_once"},
        {"turn.once", "turn_around"},
        {"idle.tappingHead", "idle_tapping_head"},
        {"idle.shrug", "idle_shrug"},
        {"idle.checkWatch", "idle_check_watch"},
        {"idle.pointing", "idle_pointing"},
        {"idle.sittingTea", "idle_sitting_tea"},
        {"idle.phoneCall", "idle_phone_call"},
        {"idle.lookBack", "idle_look_back"},
        {"idle.lookDown", "idle_look_down"},
        {"idle.lookUp", "idle_look_up"},
    };

    for (const RecipeActionCase &recipeCase : idleRecipeCases) {
        requireRecipeAction(runtime, recipeCase, "随机 idle 非移动候选应播放预期动作");
    }

    runtime.submitActionRequest(ActionRequest::recipe("walk.east"));
    QVariantMap walkDelta = runtime.consumeFrameMovementDelta();
    require(walkDelta.value("dx").toDouble() > 0, "walk.east 应推动窗口向右移动");
    require(runtime.currentFacing() == "right", "walk.east 应让桌宠朝右");

    const MovementCase movementCases[] = {
        {"east", 1, 0, "right", false},
        {"west", -1, 0, "left", false},
        {"northEast", 1, -1, "right", false},
        {"northWest", -1, -1, "left", false},
        {"southEast", 1, 1, "right", false},
        {"southWest", -1, 1, "left", false},
        {"north", 0, -1, "left", true},
        {"south", 0, 1, "left", true},
    };

    for (const MovementCase &movementCase : movementCases) {
        const QString direction = QString::fromUtf8(movementCase.direction);
        const QString walkRecipeId = QStringLiteral("walk.%1").arg(direction);
        const QString runRecipeId = QStringLiteral("run.%1").arg(direction);
        if (movementCase.preserveFacing) {
            runtime.setFacing("left");
        }

        runtime.playRecipe(walkRecipeId);
        require(runtime.currentActionId() == "walk", "walk recipe 应播放 walk action");
        require(runtime.currentMovementDirection() == direction, "移动方向应更新到 recipe 声明的方向");
        require(runtime.currentFacing() == QString::fromUtf8(movementCase.facing), movementCase.preserveFacing ? "纯纵向移动应保留原朝向" : "移动方向应更新桌宠朝向");
        const QVariantMap directionWalkDelta = runtime.consumeFrameMovementDelta();
        requireSignedDelta(directionWalkDelta.value("dx").toDouble(), movementCase.dxSign, walkRecipeId, "dx");
        requireSignedDelta(directionWalkDelta.value("dy").toDouble(), movementCase.dySign, walkRecipeId, "dy");

        runtime.playRecipe(runRecipeId);
        require(runtime.currentActionId() == "run", "run recipe 应播放 run action");
        require(runtime.currentMovementDirection() == direction, "移动方向应更新到 recipe 声明的方向");
        require(runtime.currentFacing() == QString::fromUtf8(movementCase.facing), movementCase.preserveFacing ? "纯纵向移动应保留原朝向" : "移动方向应更新桌宠朝向");
        const QVariantMap directionRunDelta = runtime.consumeFrameMovementDelta();
        requireSignedDelta(directionRunDelta.value("dx").toDouble(), movementCase.dxSign, runRecipeId, "dx");
        requireSignedDelta(directionRunDelta.value("dy").toDouble(), movementCase.dySign, runRecipeId, "dy");

        const double walkSpeed = std::abs(directionWalkDelta.value("dx").toDouble()) + std::abs(directionWalkDelta.value("dy").toDouble());
        const double runSpeed = std::abs(directionRunDelta.value("dx").toDouble()) + std::abs(directionRunDelta.value("dy").toDouble());
        require(runSpeed > walkSpeed, "run 应比 walk 移动更快");
    }

    runtime.playLocomotion("walk", "northEast");
    runtime.playRecipe("run.current");
    require(runtime.currentActionId() == "run", "run.current 应切到跑步动作");
    require(runtime.currentMovementDirection() == "northEast", "walk/run current recipe 应沿用当前方向");
    runtime.playRecipe("walk.current");
    require(runtime.currentActionId() == "walk", "walk.current 应切回走路动作");
    require(runtime.currentMovementDirection() == "northEast", "walk/run current recipe 应沿用当前方向");

    runtime.toggleAutoMovementEnabled();
    walkDelta = runtime.consumeFrameMovementDelta();
    require(walkDelta.value("dx").toDouble() == 0.0 && walkDelta.value("dy").toDouble() == 0.0, "禁止走动后移动增量应为 0");
    runtime.toggleAutoMovementEnabled();

    // 旧版拖拽晃动会在 1 秒内统计左右换向次数：达到阈值后先蹲下，
    // 松手时再根据蹲下动画是否播到末帧，选择快速站起或完整站起。
    runtime.returnToIdle();
    bridge.submitDragStarted(100);
    bridge.submitDragMoved(110);
    bridge.submitDragMoved(104);
    bridge.submitDragMoved(112);
    bridge.submitDragMoved(103);
    bridge.submitDragMoved(113);
    bridge.submitDragMoved(102);
    require(runtime.currentActionId() == "drag_crouch", "连续左右换向但未跨过初始点时也应触发晃动");
    bridge.submitDragEnded();
    runtime.handleAnimationFinished();

    runtime.returnToIdle();
    bridge.submitDragStarted(100);
    bridge.submitDragMoved(90);
    bridge.submitDragMoved(110);
    bridge.submitDragMoved(85);
    bridge.submitDragMoved(115);
    bridge.submitDragMoved(80);
    require(runtime.currentActionId() == "drag_crouch", "左右晃动达到阈值后应进入 drag_crouch");
    bridge.submitDragEnded();
    require(runtime.currentActionId() == "drag_stand_up_quick", "未蹲到底时松手应快速站起");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "快速站起播完后应回到 idle_stand");

    runtime.returnToIdle();
    bridge.submitDragStarted(100);
    bridge.submitDragMoved(90);
    bridge.submitDragMoved(110);
    bridge.submitDragMoved(85);
    bridge.submitDragMoved(115);
    bridge.submitDragMoved(80);
    require(runtime.currentActionId() == "drag_crouch", "左右晃动达到阈值后应进入 drag_crouch");
    bridge.submitHoldAnimationReachedEnd();
    bridge.submitDragEnded();
    require(runtime.currentActionId() == "drag_stand_up_full", "蹲到底后松手应完整站起");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "完整站起播完后应回到 idle_stand");

    require(hasSkinCommand(bridge.enabledSkinCommands(), "miles.feedTea"), "待机状态应允许 Miles 红茶皮肤命令");
    bridge.submitMenuCommand("miles.feedTea");
    require(runtime.currentRecipeId() == "tea.once" || runtime.currentRecipeId() == "teaAlt.once", "红茶皮肤命令应从两组喝茶 recipe 中选择");
    require(runtime.currentActionId() == "tea" || runtime.currentActionId() == "tea_alt", "喝茶 recipe 应只播放茶杯 GIF 本体");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "喝茶 GIF 播完后应直接回到待机");

    bridge.submitMenuCommand("runtime.sleep.toggle");
    require(runtime.currentActionId() == "sleep", "睡眠菜单命令应进入 sleep action");
    require(runtime.currentPhaseId() == "enter", "非睡眠状态 sleep toggle 事件应从 enter phase 开始");
    require(runtime.sleepTransitioning(), "sleep.enter 期间应视为睡眠过渡");
    require(!hasSkinCommand(bridge.enabledSkinCommands(), "miles.feedTea"), "睡眠相关状态中应禁用红茶皮肤命令");

    runtime.handleAnimationFinished();
    require(runtime.sleeping(), "sleep.enter 播完后应进入 sleeping loop");
    require(runtime.currentPhaseId() == "loop", "sleep loop phase 应为 loop");

    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(runtime.sleeping(), "睡眠中单击不应打断 sleep loop");
    bridge.submitDoubleClick();
    require(runtime.currentPhaseId() == "exit", "睡眠中双击应进入 wake/exit phase");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "wake 播完后应回到 idle_stand");
    require(hasSkinCommand(bridge.enabledSkinCommands(), "miles.feedTea"), "醒来后应重新允许红茶皮肤命令");

    bridge.submitMenuCommand("runtime.sleep.toggle");
    runtime.handleAnimationFinished();
    require(runtime.sleeping(), "再次 sleep toggle 事件后应进入 sleeping loop");
    bridge.submitMenuCommand("runtime.sleep.toggle");
    require(runtime.currentPhaseId() == "exit", "睡眠循环中 sleep toggle 事件应进入 wake/exit phase");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "wake 播完后应回到 idle_stand");
    require(hasSkinCommand(bridge.enabledSkinCommands(), "miles.feedTea"), "醒来后应重新允许红茶皮肤命令");

    runtime.playAction("click_body");
    bridge.submitMenuCommand("runtime.returnToIdle");
    require(runtime.currentActionId() == "idle_stand", "回到待机菜单事件应通过 ActionRequest 回到 idle_stand");

    const QString facingBeforeToggle = runtime.currentFacing();
    bridge.submitMenuCommand("runtime.facing.toggle");
    require(runtime.currentFacing() != facingBeforeToggle, "切换朝向菜单事件应通过 ActionRequest 切换 facing");

    require(runtime.currentAudioLanguageId() == "jp", "默认语音语言应来自 manifest.audio.defaultVoiceLanguage");
    require(runtime.availableAudioLanguages().size() == 3, "Miles 应暴露三种可选语音语言");
    runtime.setAudioLanguage("zh");
    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/holdit2.wav", "中文语音应选择 holdit2");

    runtime.setAudioLanguage("en");
    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/holdit1.wav", "英语语音应选择 holdit1");

    runtime.setAudioLanguage("jp");
    runtime.submitExpressionRequest("speaking", "objection", 0.0);
    require(runtime.currentActionId() == "objecting", "speaking + objection 应映射到异议动作");
    runtime.submitExpressionRequest("idle", "polite", 0.0);
    require(runtime.currentActionId() == "bow", "idle + polite 应映射到鞠躬动作");
    runtime.submitExpressionRequest("speaking", "unknown-expression", 0.0);
    require(runtime.currentActionId() == "idle_stand", "未知 expression 应降级到 neutral 映射");
    runtime.playAction("bow");
    runtime.submitExpressionRequest("unknown-state", "neutral", 0.0);
    require(runtime.currentActionId() == "idle_stand", "未知 expression state 应回退到当前 PetState");

    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentActionId() == "crossed", "Hold it 应播放抱臂动作");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/holdit0.wav", "Hold it 应播放默认语音");

    runtime.playRecipe("doubleClick.takeThat");
    require(runtime.currentActionId() == "objecting", "Take that 应播放异议动作");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/takethat0.wav", "Take that 应播放默认语音");

    runtime.playRecipe("doubleClick.objection");
    require(runtime.currentActionId() == "objecting", "Objection 应播放异议动作");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/objection0.wav", "Objection 应播放默认语音");

    runtime.playRecipe("doubleClick.eureka");
    require(runtime.currentActionId() == "objecting", "Eureka 应播放异议动作");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/eureka0.wav", "Eureka 应播放默认语音");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 40, 240, 240);
    require(runtime.currentActionId() == "scared", "右朝向点击脸部应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(90, 40, 240, 240);
    require(runtime.currentActionId() != "scared", "右朝向左上头部区域不应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("left");
    bridge.submitPrimaryClick(85, 40, 240, 240);
    require(runtime.currentActionId() == "scared", "左朝向点击脸部应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("left");
    bridge.submitPrimaryClick(150, 40, 240, 240);
    require(runtime.currentActionId() != "scared", "左朝向右上头部区域不应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(50, 40, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_tapping_head", "idle_look_up"}, "右朝向头部左侧不应有点击空洞");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(100, 40, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_tapping_head", "idle_look_up"}, "点击头部应触发头部候选动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(60, 85, 240, 240);
    require(runtime.currentActionId() == "turn_around", "点击大臂应触发转身");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(60, 130, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_check_watch", "idle_shrug"}, "点击小臂应触发小臂候选动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(120, 85, 240, 240);
    require(runtime.currentActionId() == "idle_thinking_once", "点击胸口应触发抱臂思考");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(120, 104, 240, 240);
    require(runtime.currentActionId() == "bow", "点击腰部上半应触发鞠躬而不是胸口动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(120, 118, 240, 240);
    require(runtime.currentActionId() == "idle_pointing", "点击肚子下半应触发指点");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(95, 200, 240, 240);
    require(runtime.currentActionId() == "back_away", "右朝向点击左侧腿部应触发后退");

    runtime.returnToIdle();
    runtime.setFacing("right");
    bridge.submitPrimaryClick(145, 200, 240, 240);
    require(runtime.currentActionId() == "idle_look_down", "右朝向点击右侧腿部应触发低头看");

    runtime.returnToIdle();
    runtime.setFacing("left");
    bridge.submitPrimaryClick(145, 200, 240, 240);
    require(runtime.currentActionId() == "back_away", "左朝向点击右侧腿部应触发后退");

    runtime.returnToIdle();
    runtime.setFacing("left");
    bridge.submitPrimaryClick(95, 200, 240, 240);
    require(runtime.currentActionId() == "idle_look_down", "左朝向点击左侧腿部应触发低头看");

    runtime.returnToIdle();
    runtime.setFacing("right");
    require(runtime.manifest().actionPools.contains("click.fallback"), "manifest 应加载 click.fallback 动作池");
    const int playbackSerialBeforeFallback = runtime.playbackSerial();
    bridge.submitPrimaryClick(10, 10, 240, 240);
    require(runtime.currentActionId() == "idle_stand", "fallback 点击应保持待机动作");
    require(runtime.playbackSerial() == playbackSerialBeforeFallback, "fallback returnToIdle 不应重启 idle_stand 动画");

    const QString soundBeforeMutedPlay = runtime.currentSoundUrl().toString();
    runtime.toggleAudioMuted();
    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentSoundUrl().toString() == soundBeforeMutedPlay, "静音时不应发出新的声音播放请求");

    runtime.returnToIdle();
    runtime.setPetSize("big");
    runtime.setFacing("right");
    runtime.playRecipe("doubleClick.takeThat");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "大号 Take that 延迟后应飞出检察官徽章");
    requireNear(runtime.currentPropStartX(), 258.0, "大号徽章右向起点应按旧版 scale=3 缩放");
    requireNear(runtime.currentPropStartY(), 48.0, "大号徽章右向纵向起点应按旧版 scale=3 缩放");
    requireNear(runtime.currentPropEndX(), 1308.0, "大号徽章右向终点应按旧版 600 + 150 * scale 计算");
    requireNear(runtime.currentPropVisualWidth(), 105.0, "大号徽章视觉尺寸应按旧版 scale=3 缩放");
    bridge.submitPropExpired();
    runtime.handleAnimationFinished();

    runtime.returnToIdle();
    runtime.setPetSize("mini");
    runtime.setFacing("left");
    runtime.playRecipe("doubleClick.takeThat");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "迷你 Take that 延迟后应飞出检察官徽章");
    requireNear(runtime.currentPropStartX(), 1.0, "迷你徽章左向起点应按旧版 scale=1 缩放");
    requireNear(runtime.currentPropStartY(), 16.0, "迷你徽章左向纵向起点应按旧版 scale=1 缩放");
    requireNear(runtime.currentPropEndX(), -749.0, "迷你徽章左向终点应按旧版 -(600 + 150 * scale) 计算");
    requireNear(runtime.currentPropVisualWidth(), 35.0, "迷你徽章视觉尺寸应按旧版 scale=1 缩放");
    bridge.submitPropExpired();
    runtime.handleAnimationFinished();

    runtime.returnToIdle();
    runtime.setPetSize("medium");
    runtime.playRecipe("doubleClick.takeThat");
    require(runtime.currentActionId() == "objecting", "Take that 应播放 objecting 动作");
    require(!runtime.currentPropVisible(), "Take that 刚触发时徽章应先等待延迟");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "Take that 延迟后应飞出检察官徽章");
    require(runtime.currentPropId() == "prosecutor_badge", "飞出的 Prop 应是检察官徽章");
    bridge.submitPropClicked();
    require(!runtime.currentPropVisible(), "点击徽章后应隐藏 Prop");
    require(runtime.currentActionId() == "bow", "点击徽章后应触发鞠躬");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "鞠躬播完后应回到 idle_stand");

    runtime.returnToIdle();
    runtime.playRecipe("doubleClick.takeThat");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "Take that 延迟后应飞出检察官徽章");
    bridge.submitPropExpired();
    require(!runtime.currentPropVisible(), "徽章自然消失后应隐藏 Prop");
    require(runtime.currentActionId() == "pickup_badge", "徽章自然消失后应触发捡徽章");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "捡徽章播完后应回到 idle_stand");

    std::cout << "PetRuntime smoke checks passed for current handfeel slice.\n";
    return 0;
}
