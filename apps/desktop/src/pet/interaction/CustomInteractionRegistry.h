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
struct CustomInteractionOutcome
{
    QList<ActionRequest> requests;
    bool continueDefault = true;
    bool stopPropagation = false;
};

using CustomInteractionResult = CustomInteractionOutcome;

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

    void emitAction(const QString &actionId);
    void emitRecipe(const QString &recipeId);
    void emitPool(const QString &poolId);
    void emitReturnToIdle();
    void spawnProp(const QString &propId, const QVariantMap &overrides = {});
    void playSound(const QUrl &url);
    void hideCurrentProp();
    double random();
    void scheduleAfter(int delayMs, std::function<void()> callback);
    RuntimeSnapshot snapshot() const;
    QVariantMap manifestConfig() const;
    void setState(const QString &key, const QVariant &value);
    QVariant getState(const QString &key) const;
    bool hasState(const QString &key) const;
    void skipDefault();
    void stopPropagation();

    CustomInteractionOutcome result() const;

private:
    const SkinManifest &m_manifest;
    RuntimeSnapshot m_snapshot;
    PetEvent m_event;
    QString m_handlerId;
    QVariantMap *m_state = nullptr;
    CustomInteractionResult m_result;
};

class CustomInteraction
{
public:
    virtual ~CustomInteraction() = default;

    virtual QString id() const = 0;
    virtual QSet<PetEventType> supportedEvents() const = 0;
    virtual CustomInteractionOutcome handleEvent(
        const PetEvent &event,
        const RuntimeSnapshot &snapshot,
        CustomInteractionHostApi &host
    ) = 0;
};

class CustomInteractionRegistry
{
public:
    static void registerInteraction(std::unique_ptr<CustomInteraction> interaction);
    static void registerBuiltins(const SkinManifest &manifest);
    static void clearForTest();
    static CustomInteractionResult handleEvent(
        const SkinManifest &manifest,
        const RuntimeSnapshot &snapshot,
        const PetEvent &event
    );
};
