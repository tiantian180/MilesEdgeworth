#pragma once
#pragma execution_character_set("utf-8")
#include <QWidget>
#include "ui_MilesEdgeworth.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMovie>
#include <QMenu>
#include <QActionGroup>
#include <QSoundEffect>
#include <QTimer>
#include <QRandomGenerator>
#include <QPropertyAnimation>
#include <QThread>
#include <QSystemTrayIcon>
#include "ProsecutorBadge/ProsecutorBadge.h"
#include "PicViewer/PicViewer.h"

const int ONCE_NUM = 20;
const int WALK_NUM = 8;
const int RUN_NUM = 8;
const double MINI = 1;
const double SMALL = 1.5;
const double MEDIAN = 2;
const double BIG = 3;
const int MAXVIEWER = 10;


class MilesEdgeworth : public QWidget
{
    Q_OBJECT

public:
    enum Type {
        RUN = 0,
        WALK,
        STAND,
        ONCE,
        SLEEP,
        SLEEPING,
        TAKETHAT,
        BRIEFCASEIN,
        BRIEFCASESTOP,
        CROUCH
    };

    explicit MilesEdgeworth(QWidget *parent = nullptr);
    ~MilesEdgeworth();

    virtual void moveEvent(QMoveEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void enterEvent(QEnterEvent* event) override;
    
    void createContextMenu();
    void createTrayIcon();
    void createBadgeTimer();
    void createShakeTimer();
    void createPicViewer();
    void showContextMenu();

    void autoChangeGif();
    void specifyChangeGif(const QString &filename, Type next_type, int next_direct);

    void runMove();
    void walkMove();

    void showProsBadge();
    void doubleClickEvent();
    void singleClickEvent();

signals:
    void soundPlay();
    void soundStop();
    void exitProgram();

public slots:
    void on_viewerClosing(bool prevIsNull, PicViewer* nextPtr);
    void on_trayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    Ui::MilesEdgeworthClass ui;
    QMovie* petMovie;
    QTimer* clickTimer;         // 用于判断单击或双击
    QTimer* badgeTimer;         // 用于让检察官徽章消失
    QTimer* shakeTimer;         // 用于判断是否触发害怕地震动作
    QSoundEffect* soundEffect;  // 用于播放语音
    ProsecutorBadge* prosbadge; // 检察官徽章
    bool m_dragging;	        // 是否正在拖动
    QPoint m_startPosition;     // 拖动开始前的鼠标位置, 是全局坐标
    QPoint m_framePosition;	    // 窗体的原始位置
    QPointF m_pressPos;         // 鼠标按下时的位置, 是widget的坐标 
    Type type;                  // 类型: run,walk,stand,once
    int direction;              // 方向: 左为1,右为0. 在走/跑时更细分为8个方向
    double scale = MEDIAN;         // 缩放倍数
    int language = 0;           // 0为日语,1为英语,2为汉语
    bool doubleClick = false;
    int shakeX = 0;                 // 用于判断是否触发害怕地震动作
    int shakeDirect = 1;            // 用于判断是否触发害怕地震动作
    int shakeCount = 0;             // 用于判断是否触发害怕地震动作, 计数move更换方向的次数

    QScreen* nowScreen = nullptr;

    QMenu* menu;
    QMenu* sizeMenu;     // 设置大小
    QMenu* languageMenu; // 设置语音语言
    QMenu* screenMenu;   // 双屏的有关设置
    QAction* actionTop;  // 是否置于顶层
    QAction* actionMove; // 是否禁止自动移动
    QAction* actionMute; // 是否静音
    QAction* actionTea;  // 喂茶
    QAction* actionSleep;// 睡觉
    QAction* actionPic;  // 显示参考图
    QAction* actionExit; // 退出

    // 子菜单的action
    QActionGroup* sizeGroup;
    QAction* miniSize;
    QAction* smallSize;
    QAction* medianSize;
    QAction* bigSize;
    QActionGroup* lanGroup;
    QAction* chLanguage;
    QAction* engLanguage;
    QAction* jpLanguage;
    QActionGroup* screenGroup;
    QAction* leftPrimary;
    QAction* rightPrimary;
    QAction* singleScreen;

    QThread* thread;
    
    // 参考图查看器多窗口使用链表结构
    PicViewer* firstViewer = nullptr;
    int numViewer = 0;

    // 系统托盘
    QSystemTrayIcon* tray;
};
