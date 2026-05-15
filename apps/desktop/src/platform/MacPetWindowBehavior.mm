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
    // 取消置顶时不能继续 orderFront，否则窗口虽然降到普通 layer，
    // 但仍会停在普通窗口队列最前面，用户体感上还是“压住别的窗口”。
    [nativeWindow orderBack:nil];
}
