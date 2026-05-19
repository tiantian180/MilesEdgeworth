#pragma once

#include <QPoint>
#include <QTimer>
#include <QWidget>

class DesktopShellController;
class QLabel;
class QContextMenuEvent;
class QMouseEvent;
class QMovie;
class QRegion;
class QSoundEffect;
class QUrl;
class PetEventBridge;
class PetRuntime;
class PropSurfaceWindow;

// PetSurfaceWindow 是桌宠本体的原生窗口。
//
// QML Window 能做视觉透明，但在 macOS/Qt Quick 下透明区域仍可能截获鼠标。
// 这里回到旧版已经验证过的 QWidget 路径：每次 GIF 帧变化时读取当前帧 alpha，
// 生成 QRegion 并调用 QWidget::setMask，让窗口系统只把不透明区域当作可交互区域。
class PetSurfaceWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PetSurfaceWindow(
        PetRuntime *runtime,
        PetEventBridge *eventBridge,
        DesktopShellController *shellController,
        QWidget *parent = nullptr
    );

    ~PetSurfaceWindow() override;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QString imagePathFromUrl(const QUrl &url) const;
    void syncSizeFromRuntime();
    void restartMovieFromRuntime();
    void handleMovieFrameChanged(int frame);
    void applyCurrentFrameMask();
    QRegion regionFromCurrentFrame() const;
    void showContextMenuAt(const QPoint &globalPosition);
    void playSoundFromRuntime();
    void showPropFromRuntime();
    void hidePropWindow();

    PetRuntime *m_runtime = nullptr;
    PetEventBridge *m_eventBridge = nullptr;
    DesktopShellController *m_shellController = nullptr;
    QLabel *m_petLabel = nullptr;
    QMovie *m_movie = nullptr;
    QSoundEffect *m_soundEffect = nullptr;
    PropSurfaceWindow *m_propWindow = nullptr;
    QTimer m_singleClickTimer;
    QTimer m_propExpireTimer;
    QPoint m_pressPosition;
    QPoint m_pendingSingleClickPosition;
    bool m_dragMoved = false;
    bool m_doubleClickPending = false;
};
