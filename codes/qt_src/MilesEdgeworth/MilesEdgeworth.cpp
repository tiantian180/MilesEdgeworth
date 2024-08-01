#include "MilesEdgeworth.h"



MilesEdgeworth::MilesEdgeworth(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    //gif动态标签
    setWindowTitle("咪酱");
    ui.petLabel->setScaledContents(true);
    ui.petLabel->resize(100 * scale, 100 * scale);
    petMovie = new QMovie(":/gifs/special/briefcaseIn0.gif");
    petMovie->setParent(this);
    type = MilesEdgeworth::BRIEFCASEIN;
    direction = 0;
    nowScreen = QGuiApplication::primaryScreen();
    QRect desktopRect = nowScreen->availableGeometry();
    move(desktopRect.x() - 45 * scale, desktopRect.y() + desktopRect.height() - 90 * scale);
    ui.petLabel->setMovie(petMovie);
    // 设置窗口为无边框 透明 置顶
    setAttribute(Qt::WA_TranslucentBackground);
    #ifdef Q_OS_MACOS
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);  
    #else
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);  
    #endif
    // 提示: 如果设置为Qt::Tool, this->close()是不会退出程序的, 只会隐藏当前窗口. 
    // 我的解决方法是close的时候发送信号, 让main里的QApplication接收到信号时调用quit()函数退出程序.
    

    createContextMenu();            // 创建右键菜单
    createTrayIcon();               // 创建托盘图标
    setContextMenuPolicy(Qt::CustomContextMenu);    // 右键唤起菜单
    createBadgeTimer();             // 创建徽章专用的计时器
    createShakeTimer();             // 创建判断是否触发害怕地震动画专用的计时器  
    petMovie->start();  
    // 创建音效播放器, 并移动到独立线程
    thread = new QThread();
    soundEffect = new QSoundEffect(this);
    soundEffect->setVolume(0.3f);
    soundEffect->moveToThread(thread);
    // 创建检察官徽章
    prosbadge = new ProsecutorBadge();
    // 连接右键菜单
    connect(this, &QWidget::customContextMenuRequested, this, &MilesEdgeworth::showContextMenu);
    // 帧切换: 走路/跑步要进行移动; 播放到最后一帧时判断是否要结束动作
    connect(petMovie, &QMovie::frameChanged, [=](int frame) {
        // 移动
        if ((type == MilesEdgeworth::RUN || type == MilesEdgeworth::BRIEFCASEIN) && !actionMove->isChecked()) {
            runMove();
        }
        else if (type == MilesEdgeworth::WALK && !actionMove->isChecked()) {
            walkMove();
        }
        // 判断是否要结束动作
        if (frame == petMovie->frameCount() - 1) { // 若此时为最后一帧
            switch (type) {
            case BRIEFCASEIN:
            {
                petMovie->setFileName(QString(":gifs/special/briefcaseStop0.gif"));
                type = MilesEdgeworth::BRIEFCASESTOP;
                direction = 0;
                break;
            }
            case BRIEFCASESTOP:
            {
                petMovie->setFileName(QString(":gifs/stand/0.gif"));
                type = MilesEdgeworth::STAND;
                direction = 0;
                break;
            }
            case MilesEdgeworth::RUN:
            case MilesEdgeworth::WALK:
            {
                // 若当前动作为跑/走, 则有二分之一的概率继续重复, 二分之一的概率停止并切换下一动作
                double randnum = QRandomGenerator::global()->generateDouble();
                if (randnum <= 0.5) {
                //petMovie->stop();
                autoChangeGif();
                }
                break;
            }
            case MilesEdgeworth::STAND:
            {
                // 若当前动作为站立, 则有0.3概率继续重复, 0.7的概率停止并切换下一动作
                double randnum = QRandomGenerator::global()->generateDouble();
                if (randnum <= 0.7) { //0.7
                    //petMovie->stop();
                    autoChangeGif();
                }
                actionSleep->setEnabled(true);
                break;
            }
            case MilesEdgeworth::TAKETHAT:
            case MilesEdgeworth::ONCE:
            {
                // 若当前动作为单次动作, 则播放一遍后立即停止, 切换下一动作
                //petMovie->stop();
                autoChangeGif();
                actionSleep->setEnabled(true);
                break;
            }
            case MilesEdgeworth::SLEEP:
            {
                // 接持续睡觉
                petMovie->setFileName(QString(":/gifs/special/sleeping%1.gif").arg(direction));
                type = MilesEdgeworth::SLEEPING;
                actionSleep->setText("唤醒");
                actionSleep->setEnabled(true);
                break;
            }
            case MilesEdgeworth::SLEEPING:
            {
                // 循环, 啥也不干.
                break;
            }
            case MilesEdgeworth::CROUCH:
            {
                // 固定在最后一帧, 直到鼠标松开才站起
                petMovie->stop();
            }
            }  
        }
        });
    // 检察官徽章被点击时消失, 触发鞠躬动画, 停止计时器
    connect(prosbadge, &ProsecutorBadge::badgeClicked, [=]() {
        prosbadge->close();
        petMovie->stop();
        specifyChangeGif(QString(":/gifs/special/bow%1.gif").arg(direction % 2),
            MilesEdgeworth::ONCE, direction % 2);
        // 停止用于定时close的计时器
        badgeTimer->stop();
        });
    // 独立线程播放音频
    connect(thread, &QThread::finished, soundEffect, &QObject::deleteLater);
    connect(soundEffect, &QObject::destroyed, thread, &QThread::deleteLater);
    connect(this, &MilesEdgeworth::soundPlay, soundEffect, &QSoundEffect::play);
    connect(this, &MilesEdgeworth::soundStop, soundEffect, &QSoundEffect::stop);
    thread->start();
}


MilesEdgeworth::~MilesEdgeworth()
{
    prosbadge->deleteLater();
    if (thread->isRunning())
    {
        qDebug() << "thread is Running";
        thread->quit();
        thread->wait();
    }
}


// 重写窗口移动事件. 用于限制窗口位置
void MilesEdgeworth::moveEvent(QMoveEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    if (QGuiApplication::screenAt(event->pos() + QPoint(50 * scale, 50 * scale)) != nullptr) {
        // 如果咪酱的中心点没被移出屏幕, 则更新咪酱的所在屏幕
        nowScreen = QGuiApplication::screenAt(event->pos() + QPoint(50 * scale, 50 * scale));
    }
    //QRect nowScreenRect = QGuiApplication::primaryScreen()->availableGeometry();
    QPoint newPos = event->pos();
    // 咪酱此时的四角坐标
    QPoint topLeft = newPos + QPoint(36 * scale, 10 * scale);
    QPoint topRight = newPos + QPoint(63 * scale, 10 * scale);
    QPoint bottomLeft = newPos + QPoint(36 * scale, 90 * scale);
    QPoint bottomRight = newPos + QPoint(63 * scale, 90 * scale);

    if (QGuiApplication::screenAt(topLeft) == nullptr || QGuiApplication::screenAt(topRight) == nullptr
        || QGuiApplication::screenAt(bottomLeft) == nullptr || QGuiApplication::screenAt(bottomRight) == nullptr) {
        // 只要有一个角出了屏幕,那么这次的move就可能要被限制
        // 窗口topLeft点的坐标限制
        int xMin;
        int xMax;
        int yMin;
        int yMax;
        QRect nowScreenRect = nowScreen->geometry();
        // 单屏时是四条边都做限制.
        xMax = nowScreenRect.x() + nowScreenRect.width() - 63 * scale;
        xMin = nowScreenRect.x() - 36 * scale;
        yMax = nowScreenRect.y() + nowScreenRect.height() - 90 * scale;
        yMin = nowScreenRect.y() - 10 * scale;
        if (leftPrimary->isChecked()) {
            // 如果主屏幕在左侧, 则不限制主屏幕右边界和第二屏幕左边界, 而是限制主屏幕的左边界和第二屏幕的右边界
            // 如果是在主屏幕内, 则有xMin但没有xMax
            if (QGuiApplication::screens().indexOf(nowScreen) == 0) {
                // 如果是在主屏幕, 则有xMin但没有xMax
                xMax = 1000000000;
            }
            else if (QGuiApplication::screens().indexOf(nowScreen) == 1) {
                // 如果是在第二屏幕, 则有xMax但没有xMin
                xMin = -1000000000;
            }
        }
        else if (rightPrimary->isChecked()) {
            // 如果主屏幕在右侧, 则不限制主屏幕左边界和第二屏幕的右边界, 而是限制主屏幕的右边界和第二屏幕的左边界
            // 如果是在主屏幕内, 则有xMax但没有xMin
            if (QGuiApplication::screens().indexOf(nowScreen) == 0) {
                // 如果是在主屏幕, 则有xMax但没有xMin
                xMin = -1000000000;
            }
            else if (QGuiApplication::screens().indexOf(nowScreen) == 1) {
                // 如果是在第二屏幕, 则有xMin但没有xMax
                xMax = 1000000000;
            }
        }

        // 限制左上角位置
        newPos.setX(qMax(xMin, newPos.x()));
        newPos.setY(qMax(yMin, newPos.y()));

        // 限制右下角位置
        newPos.setX(qMin(xMax, newPos.x()));
        newPos.setY(qMin(yMax, newPos.y()));

        move(newPos);
    }
}


void MilesEdgeworth::mouseMoveEvent(QMouseEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    if (event->buttons() & Qt::LeftButton)
    {
        //此段用于处理拖拽移动
        if (m_dragging)
        {
            //delta 相对偏移量
            QPoint delta = event->globalPosition().toPoint() - m_startPosition;
            //新位置：窗体原始位置+偏移量
            //在这里拿到屏幕的位置信息
            QScreen* screen = QGuiApplication::screenAt(event->position().toPoint());
            QRect screenGeometry = screen->geometry();
            QPoint m_newPosition = m_framePosition + delta;

            //限制窗口超出屏幕边缘
            m_newPosition.setX(qBound(screenGeometry.x() - int(36 * scale), m_newPosition.x(), screenGeometry.x() + screenGeometry.width() - int(63 * scale)));
            m_newPosition.setY(qBound(screenGeometry.y() - int(10 * scale), m_newPosition.y(), screenGeometry.y() + screenGeometry.height() - int(90 * scale)));

            //计算完毕后进行屏幕移动
            move(m_newPosition);

        }
        // 此段用于判断是否触发害怕地震动画
        if (type != MilesEdgeworth::SLEEP && type != MilesEdgeworth::SLEEPING
                && (event->globalPosition().x() - shakeX) * shakeDirect < 0) {
            // 如果移动的方向改变, 则计数+1, 记录方向和横坐标
            shakeCount++;
            shakeDirect = -shakeDirect;
            shakeX = event->globalPosition().x();
            if (shakeCount == 5 && shakeTimer->isActive()) {
                // 计数达到五次时, 触发害怕地震动画, 计时器停止, 计数清零
                petMovie->stop();
                specifyChangeGif(QString(":gifs/special/crouch%1.gif").arg(direction % 2),
                    MilesEdgeworth::CROUCH, direction % 2);
                shakeTimer->stop();
                shakeCount = 0;
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void MilesEdgeworth::mousePressEvent(QMouseEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    //响应左键
    if (event->button() == Qt::LeftButton)
    {
        // 此段用于处理拖拽移动
        m_dragging = true;
        m_startPosition = event->globalPosition().toPoint();
        m_framePosition = frameGeometry().topLeft();

        // 此段用于判断是否触发害怕地震动作
        if (type != MilesEdgeworth::SLEEP && type != MilesEdgeworth::SLEEPING) {
            shakeX = event->globalPosition().x();
            shakeCount = 0;
            shakeTimer->start();
        }
    }
    QWidget::mousePressEvent(event);
}

void MilesEdgeworth::mouseReleaseEvent(QMouseEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    // 此段用于处理拖拽移动
    m_dragging = false;
    // 此段用于处理单双击判断
    if (event->button() == Qt::LeftButton && 
        event->globalPosition().toPoint() == m_startPosition) {
        m_pressPos = event->position();
    }
    // 此段用于判断是否触发害怕地震动作
    if (type != MilesEdgeworth::SLEEP && type != MilesEdgeworth::SLEEPING) {
        shakeTimer->stop();
        shakeCount = 0;
        if (type == MilesEdgeworth::CROUCH) {
            // 如果处于蹲下状态, 则切换站起动作.
            if (petMovie->state() == QMovie::NotRunning) {
                // 如果已经蹲下, 则切换完整的站起
                specifyChangeGif(QString(":gifs/special/standup%1.gif").arg(direction),
                    MilesEdgeworth::ONCE, direction);
            }
            else {
                // 如果还没蹲下, 则切换惊吓恢复站立
                petMovie->stop();
                specifyChangeGif(QString(":gifs/special/standup%1.gif").arg(direction + 2),
                    MilesEdgeworth::ONCE, direction);
            }

        }
    }

    QWidget::mouseReleaseEvent(event);
}

void MilesEdgeworth::closeEvent(QCloseEvent* event)
{
    // delete所有picViewer
    if (firstViewer != nullptr) {
        while (firstViewer->next != nullptr) {
            PicViewer* temp = firstViewer;
            firstViewer = temp->next;
            firstViewer->prev = nullptr;
            temp->deleteLater();
        }
        firstViewer->deleteLater();
    }
    emit exitProgram();
    // QWidget::closeEvent(event);
}

void MilesEdgeworth::enterEvent(QEnterEvent* event)
{
    setCursor(Qt::PointingHandCursor);
}

void MilesEdgeworth::mouseDoubleClickEvent(QMouseEvent* event)
{
    doubleClickEvent();
}

// 创建右键菜单
void MilesEdgeworth::createContextMenu()
{
    menu = new QMenu(this);
    sizeMenu = menu->addMenu("调整大小");
    languageMenu = menu->addMenu("语音语言");
    screenMenu = menu->addMenu("双屏选项");
    menu->addSeparator();
    actionTop = menu->addAction("置于顶层");
    actionTop->setCheckable(true);  // 设置action可选中
    actionTop->setChecked(true);    // 默认选中
    actionMove = menu->addAction("禁止走动");
    actionMove->setCheckable(true);  // 设置action可选中
    actionMute = menu->addAction("静音");
    actionMute->setCheckable(true);  // 设置action可选中
    menu->addSeparator();
    actionTea = menu->addAction("喂食红茶");
    actionSleep = menu->addAction("睡觉");
    menu->addSeparator();
    actionPic = menu->addAction("图片置顶查看器");
    menu->addSeparator();
    actionExit = menu->addAction("退出");

    // 子菜单sizeMenu选项
    sizeGroup = new QActionGroup(sizeMenu);
    sizeGroup->setExclusive(true);  // 设置为单选

    miniSize = sizeMenu->addAction("迷你");
    miniSize->setCheckable(true);    // 设置action可选中
    sizeGroup->addAction(miniSize);  // 加入action组

    smallSize = sizeMenu->addAction("小");
    smallSize->setCheckable(true);    // 设置action可选中
    sizeGroup->addAction(smallSize);  // 加入action组

    medianSize = sizeMenu->addAction("中");
    medianSize->setCheckable(true);    // 设置action可选中
    medianSize->setChecked(true);      // 默认选中小
    sizeGroup->addAction(medianSize);  // 加入action组

    bigSize = sizeMenu->addAction("大");
    bigSize->setCheckable(true);    // 设置action可选中
    sizeGroup->addAction(bigSize);  // 加入action组

    // 子菜单languageMenu选项
    lanGroup = new QActionGroup(languageMenu);
    lanGroup->setExclusive(true);   // 设置为单选

    jpLanguage = languageMenu->addAction("日语");
    jpLanguage->setCheckable(true);    // 设置action可选中
    jpLanguage->setChecked(true);      // 默认选中日语
    lanGroup->addAction(jpLanguage);  // 加入action组

    engLanguage = languageMenu->addAction("英语");
    engLanguage->setCheckable(true);    // 设置action可选中
    lanGroup->addAction(engLanguage);  // 加入action组

    chLanguage = languageMenu->addAction("汉语");
    chLanguage->setCheckable(true);    // 设置action可选中
    lanGroup->addAction(chLanguage);  // 加入action组

    // 子菜单screenMenu选项
    screenGroup = new QActionGroup(screenMenu);
    screenGroup->setExclusive(true);    // 设置为单选

    singleScreen = screenMenu->addAction("单屏");
    leftPrimary = screenMenu->addAction("主屏幕在左侧");
    rightPrimary = screenMenu->addAction("主屏幕在右侧");
    singleScreen->setCheckable(true);
    singleScreen->setChecked(true);     // 默认选中单屏
    if (QGuiApplication::screens().length() == 1) {
        leftPrimary->setDisabled(true);
        rightPrimary->setDisabled(true);
    }
    else {
        leftPrimary->setCheckable(true);
        rightPrimary->setCheckable(true);
    }
    screenGroup->addAction(singleScreen);
    screenGroup->addAction(leftPrimary);
    screenGroup->addAction(rightPrimary);
    // 进行action的连接
    // 调整大小
    connect(miniSize, &QAction::triggered, [=](bool checked) {
        if (checked) {
            QPoint nowPos = frameGeometry().topLeft();
            ui.petLabel->resize(100 * MINI, 100 * MINI);
            scale = MINI;
            move(nowPos + QPoint(1, 1));  //做这一步是为了调用moveEvent,从而防止缩放过程中咪酱被移出屏幕
        }
        });
    connect(smallSize, &QAction::triggered, [=](bool checked) {
        if (checked) {
            QPoint nowPos = frameGeometry().topLeft();
            ui.petLabel->resize(100 * SMALL, 100 * SMALL);
            scale = SMALL;
            move(nowPos + QPoint(1, 1));
        }
        });
    connect(medianSize, &QAction::triggered, [=](bool checked) {
        if (checked) {
            QPoint nowPos = frameGeometry().topLeft();
            ui.petLabel->resize(100 * MEDIAN, 100 * MEDIAN);
            scale = MEDIAN;
            move(nowPos + QPoint(1, 1));
        }
        });
    connect(bigSize, &QAction::triggered, [=](bool checked) {
        if (checked) {
            QPoint nowPos = frameGeometry().topLeft();
            ui.petLabel->resize(100 * BIG, 100 * BIG);
            scale = BIG;
            move(nowPos + QPoint(1, 1));
        }
        });
    // 语音语言
    connect(jpLanguage, &QAction::triggered, [=](bool checked) {
        if (checked) {
            language = 0;
        }
        });
    connect(engLanguage, &QAction::triggered, [=](bool checked) {
        if (checked) {
            language = 1;
        }
        });
    connect(chLanguage, &QAction::triggered, [=](bool checked) {
        if (checked) {
            language = 2;
        }
        });
    // 置顶
    connect(actionTop, &QAction::triggered, [=](bool checked) {
        if (checked) {
            setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
            this->show();
        }
        else {
            setWindowFlags(this->windowFlags() & ~Qt::WindowStaysOnTopHint);
            this->show();
        }
        });
    //静音
    connect(actionMute, &QAction::triggered, [=](bool checked) {
        if (checked) {
            soundEffect->setVolume(0);
        }
        else {
            soundEffect->setVolume(0.2f);
        }
        });
    // 喂食红茶
    connect(actionTea, &QAction::triggered, [=](bool checked) {
        petMovie->stop();
        int next_direct = direction % 2;
        int randnum = QRandomGenerator::global()->bounded(2);
        specifyChangeGif(QString(":/gifs/special/tea%1.gif").arg(next_direct + randnum * 2), 
            MilesEdgeworth::ONCE, next_direct);
        });
    // 睡觉
    connect(actionSleep, &QAction::triggered, [=](bool checked) {
        if (type == MilesEdgeworth::SLEEPING) {
            // 唤醒
            petMovie->stop();
            specifyChangeGif(QString(":/gifs/special/wake%1.gif").arg(direction), 
                MilesEdgeworth::ONCE, direction);
            actionSleep->setText("睡觉");
            actionTea->setEnabled(true);
        }
        else {
            petMovie->stop();
            int next_direct = direction % 2;
            specifyChangeGif(QString(":/gifs/special/sleep%1.gif").arg(next_direct),
                MilesEdgeworth::SLEEP, next_direct);
            // 将action更名为"唤醒"
            // 在睡觉动画播放完之前, 两个按钮要被禁用
            actionSleep->setDisabled(true); // 解除禁用在frameChanged的connect里, SLEEP最后一帧时解除禁用
            actionTea->setDisabled(true);   // 唤醒时解除禁用
        }
        });
    // 置顶图查看器
    connect(actionPic, &QAction::triggered, this, &MilesEdgeworth::createPicViewer);
    // 退出: 要关掉所有参考图窗口, 彻底退出程序. 
    connect(actionExit, &QAction::triggered, this, &QWidget::close);
}


// 创建右下角托盘图标
void MilesEdgeworth::createTrayIcon()
{
    tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/icon/favicon_bar.ico"));
    QMenu* menu = new QMenu(this);
    menu->addAction(actionExit);
    tray->setContextMenu(menu);
    tray->setToolTip(windowTitle());
    //tray->show();
    tray->setVisible(true);

    connect(tray, &QSystemTrayIcon::activated, this, &MilesEdgeworth::on_trayActivated);
}

// 创建检察官徽章用的计时器
void MilesEdgeworth::createBadgeTimer()
{
    badgeTimer = new QTimer(this);
    badgeTimer->setSingleShot(true);
    badgeTimer->setInterval(FLYINGTIME);
    connect(badgeTimer, &QTimer::timeout, [=]() {
        // 超时未点击徽章, 徽章消失并且触发捡徽章动画
        prosbadge->close();
        petMovie->stop();
        specifyChangeGif(QString(":/gifs/special/pickup%1.gif").arg(direction),
            MilesEdgeworth::ONCE, direction);
        });
}

void MilesEdgeworth::createShakeTimer()
{
    shakeTimer = new QTimer(this);
    shakeTimer->setInterval(1000);  
    connect(shakeTimer, &QTimer::timeout, [=]() {
        // 1s内移动方向更换不足5次则重新计数
        shakeCount = 0;
        });
}

void MilesEdgeworth::createPicViewer()
{
    if (numViewer == MAXVIEWER) return;
    if (firstViewer == nullptr) {
        // 当前没有窗口
        firstViewer = new PicViewer();
        firstViewer->next = nullptr;
        firstViewer->prev = nullptr;
        connect(firstViewer, &PicViewer::isClosing, this, &MilesEdgeworth::on_viewerClosing);
        firstViewer->show();
    }
    else {
        // 当前已有窗口. 头插法加入新窗口
        PicViewer* temp = new PicViewer();
        temp->next = firstViewer->next;
        temp->prev = firstViewer;
        firstViewer->next = temp;
        if(temp->next != nullptr) temp->next->prev = temp;
        connect(temp, &PicViewer::isClosing, this, &MilesEdgeworth::on_viewerClosing);
        temp->show();
    }
    numViewer++;
    if (numViewer == MAXVIEWER) actionPic->setDisabled(true);   // 按钮置灰
}


// 显示右键菜单
void MilesEdgeworth::showContextMenu()
{
    menu->exec(QCursor::pos());
}


// 自动随机切换Gif
void MilesEdgeworth::autoChangeGif()
{
    Type next_type = MilesEdgeworth::STAND; // 下一动作, 默认为STAND
    int next_direct = 0;    // 下一方向, 默认为向右
    switch (type) {
    case MilesEdgeworth::RUN:
    {
        double randnum = QRandomGenerator::global()->generateDouble();
        if (randnum <= 0.2) { // 0.2概率接同向走路
            next_type = MilesEdgeworth::WALK;
            next_direct = direction;
            petMovie->setFileName(QString(":/gifs/walk/%1.gif").arg(next_direct));
        }
        else if (randnum < 0.4) {  // 0.2概率接随机跑步
            next_type = MilesEdgeworth::RUN;
            next_direct = QRandomGenerator::global()->bounded(RUN_NUM);
            petMovie->setFileName(QString(":/gifs/run/%1.gif").arg(next_direct));
        }
        else {  // 0.6概率接站立
            next_type = MilesEdgeworth::STAND;
            next_direct = direction % 2;
            petMovie->setFileName(QString(":/gifs/stand/%1.gif").arg(next_direct));
        }
        break;
    }
    case MilesEdgeworth::WALK:
    {
        double randnum = QRandomGenerator::global()->generateDouble();
        if (randnum <= 0.2) { // 0.2概率接同向跑步
            next_type = MilesEdgeworth::RUN;
            next_direct = direction;
            petMovie->setFileName(QString(":/gifs/run/%1.gif").arg(next_direct));
        }
        else if (randnum < 0.5) {  // 0.3概率接随机走路
            next_type = MilesEdgeworth::WALK;
            next_direct = QRandomGenerator::global()->bounded(WALK_NUM);
            petMovie->setFileName(QString(":/gifs/walk/%1.gif").arg(next_direct));
        }
        else {  // 0.5概率接站立
            next_type = MilesEdgeworth::STAND;
            next_direct = direction % 2;
            petMovie->setFileName(QString(":/gifs/stand/%1.gif").arg(next_direct));
        }
        break;
    }
    case MilesEdgeworth::TAKETHAT:
    case MilesEdgeworth::ONCE:
    {
        // 若当前动作为单次动作, 则停止后立刻切换为站立, 方向保持不变
        next_type = MilesEdgeworth::STAND;
        next_direct = direction % 2;
        petMovie->setFileName(QString(":/gifs/stand/%1.gif").arg(next_direct));
        break;
    }
    case MilesEdgeworth::STAND:
    {
        double randnum = QRandomGenerator::global()->generateDouble();
        if (randnum < 0.08) {
            // 0.08的概率直接切换为反向站立
            next_type = MilesEdgeworth::STAND;
            next_direct = 1 - direction;
            petMovie->setFileName(QString(":/gifs/stand/%1.gif").arg(next_direct));
        }
        else if (randnum < 0.2) {
            // 0.12的概率切换转身动画
            next_type = MilesEdgeworth::ONCE;
            next_direct = 1 - direction;
            petMovie->setFileName(QString(":/gifs/special/turn%1.gif").arg(direction));
        }
        else if (randnum < 0.8) {  //0.8
            // 0.6的概率切换为单次动作, 方向不变
            next_type = MilesEdgeworth::ONCE;
            next_direct = direction;
            int num = QRandomGenerator::global()->bounded(ONCE_NUM);
            num = (direction + 2 * num) % ONCE_NUM;
            petMovie->setFileName(QString(":/gifs/once/%1.gif").arg(num));
        }
        else if (randnum < 0.9) {  //0.9
            // 0.1的概率切换为走路
            next_type = MilesEdgeworth::WALK;
            next_direct = QRandomGenerator::global()->bounded(WALK_NUM);
            petMovie->setFileName(QString(":/gifs/walk/%1.gif").arg(next_direct));
        }
        else {
            // 0.1的概率切换为跑步
            next_type = MilesEdgeworth::RUN;
            next_direct = QRandomGenerator::global()->bounded(RUN_NUM);
            petMovie->setFileName(QString(":/gifs/run/%1.gif").arg(next_direct));
        }
        break;
    }
    }
    type = next_type;
    direction = next_direct;
    petMovie->start();
    
}

// 切换为指定的Gif. 参数为: 下一动作文件名, 下一动作类型, 下一动作方向
void MilesEdgeworth::specifyChangeGif(const QString& filename, Type next_type, int next_direct)
{
    type = next_type;
    direction = next_direct;
    petMovie->setFileName(filename);
    petMovie->start();
}


// 处理跑步时的窗口移动
void MilesEdgeworth::runMove()
{
    int unit = 4;
    QPoint delta = QPoint(0, 0);
    switch (direction) {
    case 0:
    {
        // 向右走
        delta = (QPointF(unit, 0) * scale * 1.4).toPoint();
        break;
    }
    case 1:
    {
        // 向左走
        delta = (QPointF(-unit, 0) * scale * 1.4).toPoint();
        break;
    }
    case 2:
    {
        // 向右上走
        delta = (QPointF(unit, -unit) * scale).toPoint();
        break;
    }
    case 3:
    {
        // 向左上走
        delta = (QPointF(-unit, -unit) * scale).toPoint();
        break;
    }
    case 4:
    {
        // 向右下走
        delta = (QPointF(unit, unit) * scale).toPoint();
        break;
    }
    case 5:
    {
        // 向左下走
        delta = (QPointF(-unit, unit) * scale).toPoint();
        break;
    }
    case 6:
    {
        // 向上走
        delta = (QPointF(0, -unit) * scale).toPoint();
        break;
    }
    case 7:
    {
        // 向下走
        delta = (QPointF(0, unit) * scale).toPoint();
        break;
    }
    }
    if (leftPrimary->isChecked()) {
        int primaryXMax = QGuiApplication::screens().at(0)->geometry().x() + QGuiApplication::screens().at(0)->geometry().width();
        int secondXMin = QGuiApplication::screens().at(1)->geometry().x();
        int newPosX = pos().x() + delta.x();
        if (pos().x() + 50 * scale <= primaryXMax && newPosX + 50 * scale > primaryXMax) {
            // 此时为从主屏幕跨屏到第二屏幕的瞬间
            newPosX = secondXMin - 40 * scale;
        }
        else if (pos().x() + 50 * scale >= secondXMin && newPosX + 50 * scale < secondXMin) {
            // 此时为从第二屏幕跨屏到第一屏幕的瞬间
            newPosX = primaryXMax - 60 * scale;
        }
        int newPosY = pos().y() + delta.y();
        move(newPosX, newPosY);
    }
    else if (rightPrimary->isChecked()) {
        // 主屏幕在右侧时, 跨屏时要注意坐标
        int secondXMax = QGuiApplication::screens().at(1)->geometry().x() + QGuiApplication::screens().at(1)->geometry().width();
        int primaryXMin = QGuiApplication::screens().at(0)->geometry().x();
        int newPosX = pos().x() + delta.x();
        if (pos().x() + 50 * scale >= primaryXMin && newPosX + 50 * scale < primaryXMin) {
            // 此时为从主屏幕跨屏到第二屏幕的瞬间
            newPosX = secondXMax - 60 * scale;
            qDebug() << "此时为从主屏幕跨屏到第二屏幕的瞬间, newPosX: " << newPosX;
        }
        else if (pos().x() + 50 * scale <= secondXMax && newPosX + 50 * scale > secondXMax) {
            // 此时为从第二屏幕跨屏到第一屏幕的瞬间
            newPosX = primaryXMin - 40 * scale;
            qDebug() << "此时为第二屏幕跨屏主屏幕的瞬间, newPosX: " << newPosX;
        }
        int newPosY = pos().y() + delta.y();
        move(newPosX, newPosY);
    }
    else {
        // 单屏或者主屏幕在左侧时, 正常move就行
        move(pos() + delta);
    }
}

// 处理走路时的窗口移动
void MilesEdgeworth::walkMove()
{
    int unit = 3;
    QPoint delta = QPoint(0, 0);
    switch (direction) {
    case 0:
    {
        // 向右走
        delta = (QPointF(unit, 0) * scale * 1.4).toPoint();
        break;
    }
    case 1:
    {
        // 向左走
        delta = (QPointF(-unit, 0) * scale * 1.4).toPoint();
        break;
    }
    case 2:
    {
        // 向右上走
        delta = (QPointF(unit, -unit) * scale).toPoint();
        break;
    }
    case 3:
    {
        // 向左上走
        delta = (QPointF(-unit, -unit) * scale).toPoint();
        break;
    }
    case 4:
    {
        // 向右下走
        delta = (QPointF(unit, unit) * scale).toPoint();
        break;
    }
    case 5:
    {
        // 向左下走
        delta = (QPointF(-unit, unit) * scale).toPoint();
        break;
    }
    case 6:
    {
        // 向上走
        delta = (QPointF(0, -unit) * scale).toPoint();
        break;
    }
    case 7:
    {
        // 向下走
        delta = (QPointF(0, unit) * scale).toPoint();
        break;
    }
    }
    if (leftPrimary->isChecked()) {
        int primaryXMax = QGuiApplication::primaryScreen()->geometry().x() + QGuiApplication::primaryScreen()->geometry().width();
        int secondXMin = QGuiApplication::screens().at(1)->geometry().x();
        int newPosX = pos().x() + delta.x();
        if (pos().x() + 50 * scale <= primaryXMax && newPosX + 50 * scale > primaryXMax) {
            // 此时为从主屏幕跨屏到第二屏幕的瞬间
            newPosX = secondXMin - 50 * scale;
        }
        else if (pos().x() + 50 * scale >= secondXMin && newPosX + 50 * scale < secondXMin) {
            // 此时为从第二屏幕跨屏到第一屏幕的瞬间
            newPosX = primaryXMax - 50 * scale;
        }
        int newPosY = pos().y() + delta.y();
        move(newPosX, newPosY);
    }
    else if (rightPrimary->isChecked()) {
        // 主屏幕在右侧时, 跨屏时要注意坐标
        int secondXMax = QGuiApplication::screens().at(1)->geometry().x() + QGuiApplication::screens().at(1)->geometry().width();
        int primaryXMin = QGuiApplication::screens().at(0)->geometry().x();
        int newPosX = pos().x() + delta.x();
        if (pos().x() + 50 * scale >= primaryXMin && newPosX + 50 * scale < primaryXMin) {
            // 此时为从主屏幕跨屏到第二屏幕的瞬间
            newPosX = secondXMax - 60 * scale;
            qDebug() << "此时为从主屏幕跨屏到第二屏幕的瞬间, newPosX: " << newPosX;
        }
        else if (pos().x() + 50 * scale <= secondXMax && newPosX + 50 * scale > secondXMax) {
            // 此时为从第二屏幕跨屏到第一屏幕的瞬间
            newPosX = primaryXMin - 40 * scale;
            qDebug() << "此时为第二屏幕跨屏主屏幕的瞬间, newPosX: " << newPosX;
        }
        int newPosY = pos().y() + delta.y();
        move(newPosX, newPosY);
    }
    else {
        // 单屏或者主屏幕在左侧时, 正常move就行
        move(pos() + delta);
    }
}


// 显示检察官徽章
void MilesEdgeworth::showProsBadge()
{
    QTimer::singleShot(700, [=](){ 
        if (type == MilesEdgeworth::TAKETHAT) {
            prosbadge->setScale(scale);
            if (direction == 0) {
                // 向右时, 设置初始位置
                prosbadge->setGeometry(this->geometry().x() + 86 * scale, this->geometry().y() + 16 * scale, 94, 94);
            }
            else {
                // 向左时, 设置初始位置
                prosbadge->setGeometry(this->geometry().x() + 1 * scale, this->geometry().y() + 16 * scale, 94, 94);
            }
            prosbadge->show();
            prosbadge->moveAnimation(direction);
            badgeTimer->start();
        }
        }); 
}


// 处理双击事件
void MilesEdgeworth::doubleClickEvent()
{
    QString filename;
    petMovie->stop();
    if (type == MilesEdgeworth::SLEEPING) {
        // 唤醒
        filename = QString(":/gifs/special/wake%1.gif").arg(direction);
        specifyChangeGif(filename, MilesEdgeworth::ONCE, direction);
        actionSleep->setText("睡觉");
        actionTea->setEnabled(true);
    }
    else {
        Type next_type = MilesEdgeworth::ONCE;
        double randnum = QRandomGenerator::global()->generateDouble();
        if (randnum < 0.25) {       // 等等
            filename = QString(":/gifs/special/crossed%1.gif").arg(direction % 2);
            soundEffect->setSource(QUrl::fromLocalFile(QString(":/audios/holdit%1.wav").arg(language)));
        }
        else if (randnum < 0.5) {  // 看招
            filename = QString(":/gifs/special/object%1.gif").arg(direction % 2);
            next_type = MilesEdgeworth::TAKETHAT;
            soundEffect->setSource(QUrl::fromLocalFile(QString(":/audios/takethat%1.wav").arg(language)));
            // 飞出检察官徽章
            showProsBadge();
        }
        else if (randnum < 0.75 || language == 2) {   // 异议
            filename = QString(":/gifs/special/object%1.gif").arg(direction % 2);
            soundEffect->setSource(QUrl::fromLocalFile(QString(":/audios/objection%1.wav").arg(language)));
        }
        else {                      // koreda
            filename = QString(":/gifs/special/object%1.gif").arg(direction % 2);
            soundEffect->setSource(QUrl::fromLocalFile(QString(":/audios/eureka%1.wav").arg(language)));
        }
        // 更改动画
        specifyChangeGif(filename, next_type, direction % 2);
        // 播放音频
        //soundEffect->play();
        if (soundEffect->isPlaying()) {
            emit soundStop();
        }
        emit soundPlay();
    }

}

// 处理单击事件
void MilesEdgeworth::singleClickEvent()
{
    if (type == MilesEdgeworth::SLEEPING || type == MilesEdgeworth::SLEEP) {
        // 睡觉状态下, 单击无反应.
        return;
    }
    else if (type != MilesEdgeworth::STAND) {
        // 非站立也非睡觉时, 转为站立动画
        petMovie->stop();
        specifyChangeGif(QString(":/gifs/stand/%1.gif").arg(direction % 2),
            MilesEdgeworth::STAND, direction % 2);
    }
    else {
        // 站立时, 分区域有不同的反应
        double x = m_pressPos.x();
        double y = m_pressPos.y();
        // 判断点击区域
        switch (direction) {
        case 0:
        {
            if (y < 23 * scale && y + 1.2 * x > 74 * scale) {
                // 点击脸部, 触发scared动画
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/special/scared%1.gif").arg(direction),
                    MilesEdgeworth::ONCE, direction);
            }
            else if (y < 23 * scale) {
                // 点击头部, 触发指头动画(once/2.gif once/3.gif)或抬头看动画(once/18.gif once/19.gif)
                double randnum = QRandomGenerator::global()->generateDouble();
                petMovie->stop();
                if (randnum < 0.5) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 2),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 18),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else if (x < 43 * scale && y < 40 * scale) {
                // 点击大臂, 触发转向正面小动画
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/special/turn%1.gif").arg(direction),
                    MilesEdgeworth::ONCE, 1 - direction);

            }
            else if (x < 43 * scale && y < 57 * scale) {
                // 点击小臂, 触发看表动画(once/8.gif once/9.gif)或摊手动画(once/0.gif once/1.gif)
                double randnum = QRandomGenerator::global()->generateDouble();
                petMovie->stop();
                if (randnum < 0.5) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 8),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else if (y < 38 * scale) {
                // 点击胸部, 触发抱胸手指动画(once/4.gif once/5.gif)
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 4),
                    MilesEdgeworth::ONCE, direction);
            }
            else if (y < 53 * scale) {
                // 点击肚子, 触发指指点点动画(once/10.gif once/11.gif)或鞠躬
                petMovie->stop();
                if (y > 46 * scale) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 10),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/special/bow%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else {
                // 点击腿部, 触发后退或向下看动画
                petMovie->stop();
                if (x < 50 * scale) {
                    specifyChangeGif(QString(":/gifs/special/back%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 16),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            break;
        }
        case 1:
        {
            if (y < 23 * scale && y - 1.2 * x > -43.4 * scale) {
                // 点击脸部, 触发scared动画
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/special/scared%1.gif").arg(direction),
                    MilesEdgeworth::ONCE, direction);
            }
            else if (y < 23 * scale) {
                double randnum = QRandomGenerator::global()->generateDouble();
                petMovie->stop();
                if (randnum < 0.5) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 2),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 18),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else if (x > 57 * scale && y < 40 * scale) {
                // 点击大臂, 触发转向正面小动画
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/special/turn%1.gif").arg(direction),
                    MilesEdgeworth::ONCE, 1 - direction);

            }
            else if (x > 57 * scale && y < 57 * scale) {
                // 点击小臂, 触发看表动画(once/8.gif once/9.gif)或摊手动画(once/0.gif once/1.gif)
                double randnum = QRandomGenerator::global()->generateDouble();
                petMovie->stop();
                if (randnum < 0.5) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 8),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else if (y < 38 * scale) {
                // 点击胸部, 触发抱胸手指动画(once/4.gif once/5.gif)
                petMovie->stop();
                specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 4),
                    MilesEdgeworth::ONCE, direction);
            }
            else if (y < 53 * scale) {
                // 点击肚子, 触发指指点点动画(once/10.gif once/11.gif)或鞠躬
                petMovie->stop();
                if (y > 46 * scale) {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 10),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/special/bow%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            else {
                // 点击腿部, 触发后退或向下看动画
                petMovie->stop();
                if (x > 49 * scale) {
                    specifyChangeGif(QString(":/gifs/special/back%1.gif").arg(direction),
                        MilesEdgeworth::ONCE, direction);
                }
                else {
                    specifyChangeGif(QString(":/gifs/once/%1.gif").arg(direction + 16),
                        MilesEdgeworth::ONCE, direction);
                }
            }
            break;
        }
        }
    }
}


void MilesEdgeworth::on_viewerClosing(bool prevIsNull, PicViewer* nextPtr) 
{
    numViewer--;
    if (numViewer == MAXVIEWER - 1) actionPic->setEnabled(true);
    if (prevIsNull) firstViewer = nextPtr;
}

void MilesEdgeworth::on_trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        this->activateWindow();
    }
}