#pragma once

#include <QPoint>

class DesktopShellController;
class CursorFollowController;
class ChatController;
class PetEventBridge;
class PetRuntime;
class QWidget;

// PetContextMenu 只负责把运行时状态映射成原生右键菜单。
// 它不直接播放动画；所有行为仍通过 Runtime 设置入口或 PetEventBridge 事件入口进入。
class PetContextMenu
{
public:
    static void show(
        QWidget *parent,
        PetRuntime *runtime,
        PetEventBridge *eventBridge,
        DesktopShellController *shellController,
        ChatController *chatController,
        CursorFollowController *cursorFollow,
        const QPoint &globalPosition
    );
};
