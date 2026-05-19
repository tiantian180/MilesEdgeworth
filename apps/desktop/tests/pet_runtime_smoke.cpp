#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <cmath>
#include <cstdlib>
#include <iostream>

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

    PetRuntime runtime;

    // 启动时应进入旧版公文包入场序列，而不是直接静止站立。
    require(runtime.currentRecipeId() == "startup.briefcase", "启动时应播放 startup.briefcase recipe");
    require(runtime.currentActionId() == "briefcase_in", "启动第一步应是 briefcase_in");
    require(!runtime.pointerInteractionEnabled(), "briefcase_in 期间应禁用鼠标交互");

    runtime.handlePrimaryClick(145, 40, 240, 240);
    require(runtime.currentActionId() == "briefcase_in", "启动入场期间单击不应打断 briefcase_in");

    runtime.handleDoubleClick();
    require(runtime.currentActionId() == "briefcase_in", "启动入场期间双击不应打断 briefcase_in");

    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "briefcase_stop", "公文包入场结束后应进入 briefcase_stop");
    require(runtime.pointerInteractionEnabled(), "briefcase_in 结束后应恢复鼠标交互");

    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "公文包停下后应进入 idle_stand");
    require(runtime.currentRecipeId().isEmpty(), "启动序列结束后应清空 currentRecipeId");

    runtime.handleIdleLoopFinishedForTest(0.71);
    require(runtime.currentActionId() == "idle_stand", "站立循环随机数超过 0.7 时应继续站立");
    require(runtime.currentRecipeId().isEmpty(), "站立循环随机数超过 0.7 时不应进入随机 recipe");

    runtime.handleIdleLoopFinishedForTest(0.69);
    require(!runtime.currentRecipeId().isEmpty(), "站立循环随机数不超过 0.7 时应进入随机 idle recipe");
    runtime.returnToIdle();

    runtime.playRecipe("doubleClick.holdIt");
    runtime.handleIdleLoopFinishedForTest(0.0);
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

    runtime.playRecipe("walk.east");
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
    runtime.handleDragStarted(100);
    runtime.handleDragMoved(90);
    runtime.handleDragMoved(110);
    runtime.handleDragMoved(85);
    runtime.handleDragMoved(115);
    runtime.handleDragMoved(80);
    require(runtime.currentActionId() == "drag_crouch", "左右晃动达到阈值后应进入 drag_crouch");
    runtime.handleDragEnded();
    require(runtime.currentActionId() == "drag_stand_up_quick", "未蹲到底时松手应快速站起");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "快速站起播完后应回到 idle_stand");

    runtime.returnToIdle();
    runtime.handleDragStarted(100);
    runtime.handleDragMoved(90);
    runtime.handleDragMoved(110);
    runtime.handleDragMoved(85);
    runtime.handleDragMoved(115);
    runtime.handleDragMoved(80);
    require(runtime.currentActionId() == "drag_crouch", "左右晃动达到阈值后应进入 drag_crouch");
    runtime.handleHoldAnimationReachedEnd();
    runtime.handleDragEnded();
    require(runtime.currentActionId() == "drag_stand_up_full", "蹲到底后松手应完整站起");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "完整站起播完后应回到 idle_stand");

    runtime.requestTea();
    require(runtime.currentRecipeId() == "tea.once" || runtime.currentRecipeId() == "teaAlt.once", "requestTea 应从两组喝茶 recipe 中选择");
    require(runtime.currentActionId() == "tea" || runtime.currentActionId() == "tea_alt", "喝茶 recipe 应只播放茶杯 GIF 本体");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "喝茶 GIF 播完后应直接回到待机");

    runtime.returnToIdle();
    runtime.testTea();
    require(runtime.currentRecipeId() == "tea.once" || runtime.currentRecipeId() == "teaAlt.once", "测试喝茶入口也应复用菜单喝茶候选池");
    require(runtime.currentActionId() == "tea" || runtime.currentActionId() == "tea_alt", "测试喝茶入口应播放旧版茶杯 GIF");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "测试喝茶 GIF 播完后应直接回到待机");

    runtime.toggleSleep();
    require(runtime.currentActionId() == "sleep", "toggleSleep 应进入 sleep action");
    require(runtime.currentPhaseId() == "enter", "非睡眠状态 toggleSleep 应从 enter phase 开始");
    require(runtime.sleepTransitioning(), "sleep.enter 期间应视为睡眠过渡");
    require(!runtime.teaEnabled(), "睡眠相关状态中应禁用喝茶");

    runtime.handleAnimationFinished();
    require(runtime.sleeping(), "sleep.enter 播完后应进入 sleeping loop");
    require(runtime.currentPhaseId() == "loop", "sleep loop phase 应为 loop");

    runtime.handlePrimaryClick(145, 40, 240, 240);
    require(runtime.sleeping(), "睡眠中单击不应打断 sleep loop");
    runtime.handleDoubleClick();
    require(runtime.currentPhaseId() == "exit", "睡眠中双击应进入 wake/exit phase");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "wake 播完后应回到 idle_stand");
    require(runtime.teaEnabled(), "醒来后应重新允许喝茶");

    runtime.toggleSleep();
    runtime.handleAnimationFinished();
    require(runtime.sleeping(), "再次 toggleSleep 后应进入 sleeping loop");
    runtime.toggleSleep();
    require(runtime.currentPhaseId() == "exit", "睡眠循环中 toggleSleep 应进入 wake/exit phase");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "wake 播完后应回到 idle_stand");
    require(runtime.teaEnabled(), "醒来后应重新允许喝茶");

    runtime.setVoiceLanguage("jp");
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

    runtime.setVoiceLanguage("zh");
    runtime.playRecipe("doubleClick.objection");
    require(runtime.currentSoundUrl().toString() == "qrc:/audio/objection2.wav", "中文语音应选择 objection2.wav");

    for (int i = 0; i < 80; ++i) {
        runtime.returnToIdle();
        runtime.handleDoubleClick();
        require(runtime.currentRecipeId() != "doubleClick.eureka", "中文双击不应进入 Eureka 分支");
    }

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(145, 40, 240, 240);
    require(runtime.currentActionId() == "scared", "右朝向点击脸部应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(90, 40, 240, 240);
    require(runtime.currentActionId() != "scared", "右朝向左上头部区域不应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("left");
    runtime.handlePrimaryClick(85, 40, 240, 240);
    require(runtime.currentActionId() == "scared", "左朝向点击脸部应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("left");
    runtime.handlePrimaryClick(150, 40, 240, 240);
    require(runtime.currentActionId() != "scared", "左朝向右上头部区域不应触发 scared");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(100, 40, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_tapping_head", "idle_look_up"}, "点击头部应触发头部候选动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(60, 85, 240, 240);
    require(runtime.currentActionId() == "turn_around", "点击大臂应触发转身");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(60, 130, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_check_watch", "idle_shrug"}, "点击小臂应触发小臂候选动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(120, 85, 240, 240);
    require(runtime.currentActionId() == "idle_thinking_once", "点击胸口应触发抱臂思考");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(120, 150, 240, 240);
    requireActionIn(runtime.currentActionId(), {"idle_pointing", "bow"}, "点击肚子应触发肚子候选动作");

    runtime.returnToIdle();
    runtime.setFacing("right");
    runtime.handlePrimaryClick(120, 200, 240, 240);
    requireActionIn(runtime.currentActionId(), {"back_away", "idle_look_down"}, "点击腿部应触发腿部候选动作");

    const QString soundBeforeMutedPlay = runtime.currentSoundUrl().toString();
    runtime.toggleAudioMuted();
    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentSoundUrl().toString() == soundBeforeMutedPlay, "静音时不应发出新的声音播放请求");

    runtime.returnToIdle();
    runtime.playRecipe("doubleClick.takeThat");
    require(runtime.currentActionId() == "objecting", "Take that 应播放 objecting 动作");
    require(!runtime.currentPropVisible(), "Take that 刚触发时徽章应先等待延迟");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "Take that 延迟后应飞出检察官徽章");
    require(runtime.currentPropId() == "prosecutor_badge", "飞出的 Prop 应是检察官徽章");
    runtime.handlePropClicked();
    require(!runtime.currentPropVisible(), "点击徽章后应隐藏 Prop");
    require(runtime.currentActionId() == "bow", "点击徽章后应触发鞠躬");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "鞠躬播完后应回到 idle_stand");

    runtime.returnToIdle();
    runtime.playRecipe("doubleClick.takeThat");
    waitForMilliseconds(750);
    require(runtime.currentPropVisible(), "Take that 延迟后应飞出检察官徽章");
    runtime.handlePropExpired();
    require(!runtime.currentPropVisible(), "徽章自然消失后应隐藏 Prop");
    require(runtime.currentActionId() == "pickup_badge", "徽章自然消失后应触发捡徽章");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "捡徽章播完后应回到 idle_stand");

    std::cout << "PetRuntime smoke checks passed for current handfeel slice.\n";
    return 0;
}
