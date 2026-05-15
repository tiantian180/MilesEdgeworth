#pragma once

#include <QObject>

class QWindow;

// QML 与桌面壳层之间的极薄桥接层。
// QML 负责展示菜单和响应点击，真正的平台窗口行为统一收口到这里，
// 后续系统托盘、设置中心也可以复用同一套属性和方法。
class DesktopShellController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)

public:
    explicit DesktopShellController(QObject *parent = nullptr);

    bool alwaysOnTop() const;
    void setPetWindow(QWindow *window);

public slots:
    void setAlwaysOnTop(bool alwaysOnTop);
    void toggleAlwaysOnTop();

signals:
    void alwaysOnTopChanged();

private:
    void applyCurrentLayerMode();

    QWindow *m_petWindow = nullptr;
    bool m_alwaysOnTop = true;
};
