#pragma once

class QWindow;

// 给 macOS 桌宠窗口追加平台行为：
// 1. 不因为应用失焦而隐藏。
// 2. 尽量保持在所有 Space / 全屏窗口之上。
// 3. 使用透明背景，避免普通应用窗口外观。
//
// 这个函数只在 macOS 编译。其他平台会在调用处走空实现。
void applyMacPetWindowBehavior(QWindow *window);
