#include "pet/surface/PetContextMenu.h"

#include "DesktopShellController.h"
#include "pet/PetRuntime.h"
#include "pet/events/PetEventBridge.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QVariantMap>
#include <QWidget>

namespace {
QAction *addCheckedAction(QMenu *menu, QActionGroup *group, const QString &text, bool checked)
{
    QAction *action = menu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    group->addAction(action);
    return action;
}
} // namespace

void PetContextMenu::show(
    QWidget *parent,
    PetRuntime *runtime,
    PetEventBridge *eventBridge,
    DesktopShellController *shellController,
    const QPoint &globalPosition
)
{
    QMenu menu(parent);

    const QVariantList availablePetSizes = runtime->availablePetSizes();
    if (!availablePetSizes.isEmpty()) {
        QMenu *sizeMenu = menu.addMenu(QStringLiteral("调整大小"));
        auto *sizeGroup = new QActionGroup(sizeMenu);
        sizeGroup->setExclusive(true);
        for (const QVariant &sizeValue : availablePetSizes) {
            const QVariantMap size = sizeValue.toMap();
            const QString sizeId = size.value(QStringLiteral("id")).toString();
            const QString label = size.value(QStringLiteral("label")).toString();
            if (sizeId.isEmpty()) {
                continue;
            }

            QAction *sizeAction = addCheckedAction(
                sizeMenu,
                sizeGroup,
                label.isEmpty() ? sizeId : label,
                runtime->petSizeId() == sizeId
            );
            QObject::connect(sizeAction, &QAction::triggered, parent, [runtime, sizeId]() {
                runtime->setPetSize(sizeId);
            });
        }
    }

    QMenu *screenMenu = menu.addMenu(QStringLiteral("双屏选项"));
    auto *screenGroup = new QActionGroup(screenMenu);
    screenGroup->setExclusive(true);
    QAction *singleScreenAction = addCheckedAction(screenMenu, screenGroup, QStringLiteral("单屏"), shellController->screenLayoutMode() == "single");
    QAction *primaryLeftAction = addCheckedAction(screenMenu, screenGroup, QStringLiteral("主屏幕在左侧"), shellController->screenLayoutMode() == "primaryLeft");
    QAction *primaryRightAction = addCheckedAction(screenMenu, screenGroup, QStringLiteral("主屏幕在右侧"), shellController->screenLayoutMode() == "primaryRight");
    primaryLeftAction->setEnabled(shellController->screenCount() > 1);
    primaryRightAction->setEnabled(shellController->screenCount() > 1);
    QObject::connect(singleScreenAction, &QAction::triggered, parent, [shellController]() { shellController->setScreenLayoutMode(QStringLiteral("single")); });
    QObject::connect(primaryLeftAction, &QAction::triggered, parent, [shellController]() { shellController->setScreenLayoutMode(QStringLiteral("primaryLeft")); });
    QObject::connect(primaryRightAction, &QAction::triggered, parent, [shellController]() { shellController->setScreenLayoutMode(QStringLiteral("primaryRight")); });

    menu.addSeparator();
    QAction *topAction = menu.addAction(shellController->alwaysOnTop() ? QStringLiteral("取消置顶") : QStringLiteral("始终置顶"));
    QObject::connect(topAction, &QAction::triggered, shellController, &DesktopShellController::toggleAlwaysOnTop);

    QAction *movementAction = menu.addAction(QStringLiteral("禁止走动"));
    movementAction->setCheckable(true);
    movementAction->setChecked(!runtime->autoMovementEnabled());
    QObject::connect(movementAction, &QAction::triggered, runtime, &PetRuntime::toggleAutoMovementEnabled);

    QAction *muteAction = menu.addAction(QStringLiteral("静音"));
    muteAction->setCheckable(true);
    muteAction->setChecked(runtime->audioMuted());
    QObject::connect(muteAction, &QAction::triggered, runtime, &PetRuntime::toggleAudioMuted);

    const QVariantList skinCommands = eventBridge->enabledSkinCommands();
    if (!skinCommands.isEmpty()) {
        menu.addSeparator();
        QMenu *skinCommandMenu = menu.addMenu(QStringLiteral("皮肤动作"));
        for (const QVariant &commandValue : skinCommands) {
            const QVariantMap command = commandValue.toMap();
            const QString commandId = command.value(QStringLiteral("id")).toString();
            const QString label = command.value(QStringLiteral("label")).toString();
            QAction *commandAction = skinCommandMenu->addAction(label.isEmpty() ? commandId : label);
            QObject::connect(commandAction, &QAction::triggered, parent, [eventBridge, commandId]() {
                eventBridge->submitMenuCommand(commandId);
            });
        }
    }

    if (runtime->restCapabilityEnabled()) {
        QAction *sleepAction = menu.addAction(runtime->sleeping() ? QStringLiteral("唤醒") : QStringLiteral("睡觉"));
        sleepAction->setEnabled(!runtime->sleepTransitioning());
        QObject::connect(sleepAction, &QAction::triggered, parent, [eventBridge]() {
            eventBridge->submitMenuCommand(QStringLiteral("runtime.sleep.toggle"));
        });
    }

    menu.addSeparator();
    QAction *quitAction = menu.addAction(QStringLiteral("退出"));
    QObject::connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    menu.exec(globalPosition);
}
