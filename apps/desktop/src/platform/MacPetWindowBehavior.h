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

// 聊天窗口是普通工作窗口，打开时应让应用显示在 Dock；关闭后恢复桌宠
// 辅助应用模式，避免只剩桌宠本体时占用普通应用位置。
void setMacApplicationDockVisible(bool visible);
