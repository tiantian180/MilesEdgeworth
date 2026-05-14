#include "platform/MacPetWindowBehavior.h"

#include <QWindow>
#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

namespace {
using SLSMainConnectionIDFn = int (*)();
using SLSSpaceCreateFn = int (*)(int, int, int);
using SLSSpaceSetAbsoluteLevelFn = int (*)(int, int, int);
using SLSShowSpacesFn = int (*)(int, void *);
using SLSSpaceAddWindowsAndRemoveFromSpacesFn = int (*)(int, int, void *, int);

struct SkyLightSpace {
    int connection = 0;
    int space = 0;
    SLSSpaceAddWindowsAndRemoveFromSpacesFn addWindowsAndRemoveFromSpaces = nullptr;
    bool available = false;
};

SkyLightSpace &stationarySkyLightSpace()
{
    static SkyLightSpace state;
    static bool initialized = false;
    if (initialized) {
        return state;
    }
    initialized = true;

    void *skyLight = dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/SkyLight", RTLD_LAZY);
    if (skyLight == nullptr) {
        return state;
    }

    auto mainConnection = reinterpret_cast<SLSMainConnectionIDFn>(dlsym(skyLight, "SLSMainConnectionID"));
    auto spaceCreate = reinterpret_cast<SLSSpaceCreateFn>(dlsym(skyLight, "SLSSpaceCreate"));
    auto spaceSetAbsoluteLevel = reinterpret_cast<SLSSpaceSetAbsoluteLevelFn>(dlsym(skyLight, "SLSSpaceSetAbsoluteLevel"));
    auto showSpaces = reinterpret_cast<SLSShowSpacesFn>(dlsym(skyLight, "SLSShowSpaces"));
    auto addWindowsAndRemoveFromSpaces = reinterpret_cast<SLSSpaceAddWindowsAndRemoveFromSpacesFn>(
        dlsym(skyLight, "SLSSpaceAddWindowsAndRemoveFromSpaces"));

    if (mainConnection == nullptr
        || spaceCreate == nullptr
        || spaceSetAbsoluteLevel == nullptr
        || showSpaces == nullptr
        || addWindowsAndRemoveFromSpaces == nullptr) {
        return state;
    }

    state.connection = mainConnection();
    state.space = spaceCreate(state.connection, 1, 0);
    if (state.connection == 0 || state.space == 0) {
        return state;
    }

    NSArray *spaces = @[ @(state.space) ];
    spaceSetAbsoluteLevel(state.connection, state.space, 100);
    showSpaces(state.connection, (__bridge void *)spaces);

    state.addWindowsAndRemoveFromSpaces = addWindowsAndRemoveFromSpaces;
    state.available = true;
    return state;
}

void moveWindowToStationarySkyLightSpace(NSWindow *window)
{
    if (window == nil) {
        return;
    }

    SkyLightSpace &state = stationarySkyLightSpace();
    if (!state.available || state.addWindowsAndRemoveFromSpaces == nullptr) {
        return;
    }

    const NSInteger windowNumber = [window windowNumber];
    if (windowNumber <= 0) {
        return;
    }

    NSArray *windows = @[ @(windowNumber) ];
    state.addWindowsAndRemoveFromSpaces(state.connection, state.space, (__bridge void *)windows, 7);
}
}

void applyMacPetWindowBehavior(QWindow *window)
{
    if (window == nullptr) {
        return;
    }

    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = [view window];
    if (nativeWindow == nil) {
        return;
    }

    [nativeWindow setHidesOnDeactivate:NO];
    [nativeWindow setCanHide:NO];
    [nativeWindow setRestorable:NO];
    [nativeWindow setOpaque:NO];
    [nativeWindow setBackgroundColor:[NSColor clearColor]];
    [nativeWindow setMovable:NO];
    [nativeWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];

    NSWindowCollectionBehavior behavior = [nativeWindow collectionBehavior];
    behavior &= ~(NSWindowCollectionBehaviorMoveToActiveSpace
                  | NSWindowCollectionBehaviorManaged
                  | NSWindowCollectionBehaviorTransient
                  | NSWindowCollectionBehaviorParticipatesInCycle
                  | NSWindowCollectionBehaviorFullScreenPrimary
                  | NSWindowCollectionBehaviorFullScreenNone
                  | NSWindowCollectionBehaviorFullScreenAllowsTiling);
    if (@available(macOS 13.0, *)) {
        behavior &= ~NSWindowCollectionBehaviorCanJoinAllApplications;
    }
    behavior |= NSWindowCollectionBehaviorCanJoinAllSpaces
                | NSWindowCollectionBehaviorStationary
                | NSWindowCollectionBehaviorFullScreenAuxiliary
                | NSWindowCollectionBehaviorIgnoresCycle
                | NSWindowCollectionBehaviorFullScreenDisallowsTiling;
    [nativeWindow setCollectionBehavior:behavior];

    [nativeWindow setLevel:CGWindowLevelForKey(kCGAssistiveTechHighWindowLevelKey)];
    moveWindowToStationarySkyLightSpace(nativeWindow);
    [nativeWindow orderFrontRegardless];
}
