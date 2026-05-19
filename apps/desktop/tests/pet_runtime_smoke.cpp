#include "pet/PetRuntime.h"

#include <QCoreApplication>
#include <QVariantMap>

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

    runtime.playRecipe("walk.east");
    QVariantMap walkDelta = runtime.consumeFrameMovementDelta();
    require(walkDelta.value("dx").toDouble() > 0, "walk.east 应推动窗口向右移动");
    require(runtime.currentFacing() == "right", "walk.east 应让桌宠朝右");

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

    runtime.requestTea();
    require(runtime.currentRecipeId() == "tea.once" || runtime.currentRecipeId() == "teaAlt.once", "requestTea 应从两组喝茶 recipe 中选择");
    require(runtime.currentActionId() == "tea" || runtime.currentActionId() == "tea_alt", "喝茶 recipe 应只播放茶杯 GIF 本体");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "喝茶 GIF 播完后应直接回到待机");

    runtime.toggleSleep();
    require(runtime.currentActionId() == "sleep", "toggleSleep 应进入 sleep action");
    require(runtime.currentPhaseId() == "enter", "非睡眠状态 toggleSleep 应从 enter phase 开始");
    require(runtime.sleepTransitioning(), "sleep.enter 期间应视为睡眠过渡");
    require(!runtime.teaEnabled(), "睡眠相关状态中应禁用喝茶");

    runtime.handleAnimationFinished();
    require(runtime.sleeping(), "sleep.enter 播完后应进入 sleeping loop");
    require(runtime.currentPhaseId() == "loop", "sleep loop phase 应为 loop");

    runtime.toggleSleep();
    require(runtime.currentPhaseId() == "exit", "睡眠循环中 toggleSleep 应进入 wake/exit phase");
    runtime.handleAnimationFinished();
    require(runtime.currentActionId() == "idle_stand", "wake 播完后应回到 idle_stand");
    require(runtime.teaEnabled(), "醒来后应重新允许喝茶");

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

    const QString soundBeforeMutedPlay = runtime.currentSoundUrl().toString();
    runtime.toggleAudioMuted();
    runtime.playRecipe("doubleClick.holdIt");
    require(runtime.currentSoundUrl().toString() == soundBeforeMutedPlay, "静音时不应发出新的声音播放请求");

    runtime.handlePropExpired();
    runtime.playRecipe("doubleClick.takeThat");
    require(runtime.currentActionId() == "objecting", "Take that 应播放 objecting 动作");

    std::cout << "PetRuntime smoke checks passed for current handfeel slice.\n";
    return 0;
}
