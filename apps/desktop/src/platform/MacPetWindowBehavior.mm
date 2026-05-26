#include "platform/MacPetWindowBehavior.h"

#include <QWindow>

#import <Cocoa/Cocoa.h>

namespace {
NSWindow *nativeWindowForQWindow(QWindow *window)
{
    if (window == nullptr) {
        return nil;
    }

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    return [view window];
}

NSWindowCollectionBehavior baseCollectionBehavior(NSWindow *window)
{
    NSWindowCollectionBehavior behavior = [window collectionBehavior];
    behavior &= ~(NSWindowCollectionBehaviorManaged
                  | NSWindowCollectionBehaviorTransient
                  | NSWindowCollectionBehaviorParticipatesInCycle
                  | NSWindowCollectionBehaviorFullScreenPrimary
                  | NSWindowCollectionBehaviorFullScreenNone
                  | NSWindowCollectionBehaviorStationary
                  | NSWindowCollectionBehaviorCanJoinAllSpaces
                  | NSWindowCollectionBehaviorFullScreenAuxiliary
                  | NSWindowCollectionBehaviorFullScreenAllowsTiling);
    if (@available(macOS 13.0, *)) {
        behavior &= ~NSWindowCollectionBehaviorCanJoinAllApplications;
    }

    behavior |= NSWindowCollectionBehaviorIgnoresCycle
                | NSWindowCollectionBehaviorFullScreenDisallowsTiling;
    return behavior;
}

NSWindowCollectionBehavior companionCollectionBehavior(NSWindow *window)
{
    NSWindowCollectionBehavior behavior = baseCollectionBehavior(window);
    behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces
                | NSWindowCollectionBehaviorStationary
                | NSWindowCollectionBehaviorFullScreenAuxiliary;
    if (@available(macOS 13.0, *)) {
        behavior |= NSWindowCollectionBehaviorCanJoinAllApplications;
    }
    return behavior;
}
}

void applyMacPetWindowBaseBehavior(QWindow *window)
{
    NSWindow *nativeWindow = nativeWindowForQWindow(window);
    if (nativeWindow == nil) {
        return;
    }

    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    [nativeWindow setHidesOnDeactivate:NO];
    [nativeWindow setCanHide:NO];
    [nativeWindow setRestorable:NO];
    [nativeWindow setOpaque:NO];
    [nativeWindow setBackgroundColor:[NSColor clearColor]];
    [nativeWindow setMovable:NO];
    [nativeWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
}

void setMacPetWindowAlwaysOnTop(QWindow *window, bool alwaysOnTop)
{
    NSWindow *nativeWindow = nativeWindowForQWindow(window);
    if (nativeWindow == nil) {
        return;
    }

    // 这两项始终保留：即使用户取消置顶，也不希望桌宠因为应用失焦被系统隐藏。
    [nativeWindow setHidesOnDeactivate:NO];
    [nativeWindow setCanHide:NO];

    NSWindowCollectionBehavior behavior = baseCollectionBehavior(nativeWindow);
    if (alwaysOnTop) {
        behavior &= ~NSWindowCollectionBehaviorMoveToActiveSpace;
        behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces
                    | NSWindowCollectionBehaviorStationary
                    | NSWindowCollectionBehaviorFullScreenAuxiliary;
        if (@available(macOS 13.0, *)) {
            // macOS 13 之后可以声明跨应用 Space。配合辅助应用模式和 screen saver
            // level，普通置顶模式就能覆盖大多数全屏窗口，同时仍然可以降回普通窗口。
            behavior |= NSWindowCollectionBehaviorCanJoinAllApplications;
        }
        [nativeWindow setCollectionBehavior:behavior];
        [nativeWindow setLevel:CGWindowLevelForKey(kCGScreenSaverWindowLevelKey)];
        [nativeWindow orderFrontRegardless];
        return;
    }

    behavior |= NSWindowCollectionBehaviorMoveToActiveSpace;
    [nativeWindow setCollectionBehavior:behavior];
    [nativeWindow setLevel:NSNormalWindowLevel];
    // 取消置顶只改变之后的覆盖规则，不主动把桌宠丢到窗口栈底部。
    // 这样用户手滑点错时，桌宠仍停在原地，后续自然可以被其他窗口覆盖。
}

void prepareMacPetWindowForContextMenu(QWindow *window)
{
    NSWindow *nativeWindow = nativeWindowForQWindow(window);
    if (nativeWindow == nil) {
        return;
    }

    [nativeWindow orderFrontRegardless];
}

void applyMacCompanionWindowBehavior(QWindow *window)
{
    NSWindow *nativeWindow = nativeWindowForQWindow(window);
    if (nativeWindow == nil) {
        return;
    }

    [nativeWindow setHidesOnDeactivate:NO];
    [nativeWindow setCanHide:NO];
    [nativeWindow setRestorable:NO];
    [nativeWindow setOpaque:NO];
    [nativeWindow setBackgroundColor:[NSColor clearColor]];
    [nativeWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
    [nativeWindow setCollectionBehavior:companionCollectionBehavior(nativeWindow)];
    [nativeWindow setLevel:CGWindowLevelForKey(kCGScreenSaverWindowLevelKey)];
}

void prepareMacCompanionWindowForOpen(QWindow *window)
{
    NSWindow *nativeWindow = nativeWindowForQWindow(window);
    if (nativeWindow == nil) {
        return;
    }

    applyMacCompanionWindowBehavior(window);
    [nativeWindow orderFrontRegardless];
    [nativeWindow makeKeyAndOrderFront:nil];
}

void setMacApplicationDockVisible(bool visible)
{
    [NSApp setActivationPolicy:visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
}
