#pragma once

#include <QRegion>
#include <QUrl>

class QWindow;

// WindowInputMaskController 用图像 alpha 通道计算窗口的输入 mask。
//
// 桌宠窗口必须透明置顶，但 Qt/macOS 默认会让"透明像素仍接收鼠标"。
// 这里读取当前动画首帧 alpha，生成 QRegion 并下发到 QWindow，让窗口系统
// 只把不透明像素当作可交互区域；透明边距不会拦截 / 拖拽下层桌面。
//
// alphaThreshold 控制阈值（默认 8），低于此值的像素视为完全透明。
class WindowInputMaskController
{
public:
    static void applyMask(
        QWindow *window,
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold = 8
    );

    static void clearMask(QWindow *window);

private:
    static QRegion regionFromImage(
        const QUrl &animationUrl,
        double imageSize,
        double windowSize,
        int alphaThreshold
    );
};
