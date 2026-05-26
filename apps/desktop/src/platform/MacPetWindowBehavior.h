#pragma once

class QWindow;

// 给 macOS 桌宠窗口追加基础平台行为：
// 1. 不因为应用失焦而隐藏。
// 2. 使用透明背景，避免普通应用窗口外观。
// 3. 使用辅助应用模式，减少 Dock / 普通应用切换行为的干扰。
//
// “是否始终置顶”不放在这里处理，因为它是用户可切换设置。
void applyMacPetWindowBaseBehavior(QWindow *window);

// 切换桌宠窗口的层级模式。
// alwaysOnTop 为 true 时，使用标准 AppKit 窗口层级尽量覆盖所有 Space / 全屏窗口。
// alwaysOnTop 为 false 时，保留桌宠基础行为，但允许其他普通窗口覆盖它。
//
// 注意：这里故意不使用 SkyLight 私有 Space。SkyLight 更适合未来单独做
// “固定在屏幕最上层”的实验模式，不适合这个需要可随时取消的普通开关。
void setMacPetWindowAlwaysOnTop(QWindow *window, bool alwaysOnTop);

// 右键菜单显示前调用：确保 accessory app 和跨 Space 桌宠窗口在当前
// Space 成为可接收菜单的前台上下文。
void prepareMacPetWindowForContextMenu(QWindow *window);

// 聊天窗和桌宠回复气泡不是桌宠本体，但用户会从桌宠所在的当前 Space
// 打开它们。它们需要和桌宠一样加入所有 Space，并允许显示在全屏应用上方，
// 否则在桌宠跟随到全屏 Space 后，聊天 UI 会留在原来的桌面 Space。
void applyMacCompanionWindowBehavior(QWindow *window);

// 打开聊天窗前调用：确保 native 窗口已经具备 companion 行为，并进入
// 当前 Space 的前台窗口栈。
void prepareMacCompanionWindowForOpen(QWindow *window);

// 调整整个应用的 Dock / activation policy。当前只在启动时进入 accessory
// 模式；聊天窗显隐不能调用它，否则 macOS 会把全屏 Space 中的操作带回
// 应用原来的桌面 Space。
void setMacApplicationDockVisible(bool visible);
