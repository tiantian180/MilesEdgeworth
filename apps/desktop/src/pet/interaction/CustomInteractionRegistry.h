#pragma once

#include "pet/events/PetEvent.h"
#include "pet/manifest/SkinManifest.h"
#include "pet/requests/ActionRequest.h"
#include "pet/runtime/RuntimeSnapshot.h"

#include <functional>
#include <QList>
#include <QSet>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

#include <memory>

// CustomInteractionOutcome 把“是否继续默认逻辑”和“是否产生请求”分开。
//
// 高级交互可以只追加效果并继续默认行为，也可以完全接管事件。
// 这个边界后续会承接 JS/TS 或 C++ 皮肤脚本；当前阶段只保留框架口子。
//
// 字段语义：
// - requests：handler 想发出的播放请求列表（追加到管线默认产物之前）
// - continueDefault：false 时跳过 manifest 默认行为（clickBehaviors / behaviorRules）
// - stopPropagation：true 时跳过 Registry 后续注册的 handler
struct CustomInteractionOutcome
{
    QList<ActionRequest> requests;
    bool continueDefault = true;
    bool stopPropagation = false;
};

using CustomInteractionResult = CustomInteractionOutcome;

// CustomInteractionHostApi 是 handler 在每次事件分发时获得的"受控副本"。
//
// 它不暴露 PetRuntime 指针或 manifest 写权限；所有副作用通过 emit* / spawnProp /
// playSound / scheduleAfter / setState 这些动词表达。这套设计是语言无关的，
// 后续接入 JS / TS 沙箱时只需要给 Host API 写一层 adapter，handler 接口不必动。
class CustomInteractionHostApi
{
public:
    CustomInteractionHostApi(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const PetEvent &event,
        const QString &handlerId,
        QVariantMap *state
    );

    // ---- 生成 ActionRequest（不立即执行，会在 Outcome 返回时统一交给 pipeline）----
    void emitAction(const QString &actionId);
    void emitRecipe(const QString &recipeId);
    void emitPool(const QString &poolId);
    void emitReturnToIdle();

    // ---- Prop 与副作用 ----
    void spawnProp(const QString &propId, const QVariantMap &overrides = {});
    void playSound(const QUrl &url);
    void hideCurrentProp();

    // ---- 随机与计时 ----
    // 若事件携带了 randomValue（如 idle.loopFinished），返回该确定性值；否则用全局随机源。便于测试。
    double random();
    // 在 GUI 线程上 delayMs 毫秒后回调一次。底层用 QTimer::singleShot；handler 不能自起线程。
    void scheduleAfter(int delayMs, std::function<void()> callback);

    // ---- 状态读取 ----
    RuntimeSnapshot snapshot() const;
    // 读 manifest.customInteractionConfig[<本 handler id>]，让作者把 handler 参数写在 manifest 里。
    QVariantMap manifestConfig() const;

    // ---- per-handler 内存 KV（同一 handler 在多次事件之间共享，但进程重启后丢失）----
    void setState(const QString &key, const QVariant &value);
    QVariant getState(const QString &key) const;
    bool hasState(const QString &key) const;

    // ---- 流程控制 ----
    void skipDefault();      // 等价 outcome.continueDefault = false
    void stopPropagation();  // 等价 outcome.stopPropagation = true

    CustomInteractionOutcome result() const;

private:
    const SkinManifest &m_manifest;
    RuntimeSnapshot m_snapshot;
    PetEvent m_event;
    QString m_handlerId;
    QVariantMap *m_state = nullptr;
    CustomInteractionResult m_result;
};

// CustomInteraction 是皮肤包的代码扩展点。
//
// SkinCommand 解决"菜单点一下就播一个池子"；CustomInteraction 解决需要
// 概率、状态机、延时、多事件协作的高级玩法（如双击概率丢徽章、奉茶后接 bow 等）。
//
// 当前以 C++ 抽象类形式存在；后续可在保留同一套 Host API 的前提下扩展 JS / TS 沙箱执行体。
class CustomInteraction
{
public:
    virtual ~CustomInteraction() = default;

    // 唯一 id；用于日志、状态隔离和 manifest 引用（doubleClick 列表 / customInteractionConfig 都按 id 关联）。
    virtual QString id() const = 0;

    // 声明 handler 关心哪些事件类型，Registry 据此预先过滤分发开销。
    virtual QSet<PetEventType> supportedEvents() const = 0;

    // 真正的处理入口。返回 Outcome 描述本次的产出和流程控制。
    virtual CustomInteractionOutcome handleEvent(
        const PetEvent &event,
        const RuntimeSnapshot &snapshot,
        CustomInteractionHostApi &host
    ) = 0;
};

// CustomInteractionRegistry 把已注册的 handler 收集起来，并按事件分发。
//
// 设计约束：
// - handler 列表是进程级 static（当前架构假设单 PetRuntime）
// - registerBuiltins(manifest) 按 manifest.customInteractions 列表注册框架内置 handler
// - 同 id 的 handler 不会被重复注册（dedup）
// - handler 抛异常时 Registry 吞掉，并继续默认逻辑，避免高级交互崩溃影响主流程
class CustomInteractionRegistry
{
public:
    static void registerInteraction(std::unique_ptr<CustomInteraction> interaction);
    static void registerBuiltins(const SkinManifest &manifest);
    static void reset();
    static void clearForTest();

    // 把事件依次发给注册的 handler，合并它们的产出并返回最终 Outcome。
    // pipeline 会先调用这个方法，再根据 continueDefault 决定要不要走默认逻辑。
    static CustomInteractionResult handleEvent(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const PetEvent &event
    );
};
