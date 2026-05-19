#pragma once

#include <QObject>
#include <QPointF>
#include <QJSEngine>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class QAction;
class QMenu;
class QSystemTrayIcon;
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
    ~DesktopShellController() override;

    bool alwaysOnTop() const;
    void setPetWindow(QWindow *window);

public slots:
    void setAlwaysOnTop(bool alwaysOnTop);
    void setPetScale(double petScale);
    void toggleAlwaysOnTop();
    Q_INVOKABLE void revealPetWindow();
    Q_INVOKABLE void placePetWindowForStartup(double petScale);
    Q_INVOKABLE void movePetWindowBy(double dx, double dy);
    Q_INVOKABLE void movePetWindowTo(double x, double y);

signals:
    void alwaysOnTopChanged();

private:
    void createTrayIcon();
    void applyCurrentLayerMode();
    QPointF legacyStartupPosition(double petScale) const;
    QPointF clampedPetWindowPosition(const QPointF &candidatePosition) const;

    QWindow *m_petWindow = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_exitAction = nullptr;
    double m_petScale = 2.0;
    bool m_alwaysOnTop = true;
};

// 这个 wrapper 只负责告诉 QML 类型系统：
// 已经存在的 DesktopShellController 对象，要以 DesktopShell 单例暴露给 QML。
//
// 这样比 setContextProperty 更清晰：QML Language Server 能看到属性和方法，
// 后续 QML 里使用 DesktopShell.alwaysOnTop 时也不再像访问“空气变量”。
struct DesktopShellControllerForeign
{
    Q_GADGET
    QML_FOREIGN(DesktopShellController)
    QML_NAMED_ELEMENT(DesktopShell)
    QML_SINGLETON

public:
    inline static DesktopShellController *s_instance = nullptr;

    static DesktopShellController *create(QQmlEngine *, QJSEngine *scriptEngine)
    {
        Q_ASSERT(s_instance != nullptr);
        Q_ASSERT(scriptEngine->thread() == s_instance->thread());

        // 单例对象由 main.cpp 持有，QML 引擎只借用，不负责 delete。
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
};
