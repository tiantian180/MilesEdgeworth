#pragma once

#include <QPixmap>
#include <QSize>
#include <QWidget>

#include <functional>

// PropSurfaceWindow 显示主体窗口之外的临时视觉对象。
//
// 它和 PetSurfaceWindow 使用同一套 alpha mask 思路：只让图片不透明区域
// 接收鼠标事件。具体 Prop 的出现时机和后续动作仍由 PetRuntime / PetEventBridge 控制。
class PropSurfaceWindow final : public QWidget
{
public:
    explicit PropSurfaceWindow(QWidget *parent = nullptr);

    void showPixmap(
        const QString &path,
        const QSize &windowSize,
        const QSize &visualSize,
        const QPoint &globalPosition
    );
    void setClickedCallback(std::function<void()> callback);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void applyPixmapMask();

    QPixmap m_pixmap;
    QSize m_visualSize;
    std::function<void()> m_clickedCallback;
};
