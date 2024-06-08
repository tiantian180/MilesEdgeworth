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
    QRect desktopRect = QGuiApplication::primaryScreen()->availableGeometry();
    move(desktopRect.x() - 45 * scale, desktopRect.y() + desktopRect.height() / 2);
    ui.petLabel->setMovie(petMovie);
    // 设置窗口为无边框 透明 置顶
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);  
    // 提示: 如果设置为Qt::Tool, this->close()是不会退出程序的, 只会隐藏当前窗口. 
    // 我的解决方法是close的时候发送信号, 让main里的QApplication接收到信号时调用quit()函数退出程序.
    
    // 创建右键菜单
    createContextMenu();
    // 创建托盘图标
    createTrayIcon();
    setContextMenuPolicy(Qt::CustomContextMenu);
    // 创建单双击专用的计时器
    createClickTimer();
    // 创建徽章专用的计时器
    createBadgeTimer();
    petMovie->start();
    // 创建音效播放器, 并移动到独立线程
    thread = new QThread();
    soundEffect = new QSoundEffect(this);
    soundEffect->setVolume(0.3);
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
    // 窗口topLeft点的坐标限制
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    QRect desktopRect = QGuiApplication::primaryScreen()->availableGeometry();
    QPoint newPos = event->pos();
    if (scale == MINI) {
        xMax = desktopRect.width() + desktopRect.x() - 61;
        yMax = desktopRect.height() + desktopRect.y() - 50;
        xMin = desktopRect.x() - 36;
        yMin = desktopRect.y() - 9;
    }
    else if (scale == SMALL) {
        xMax = desktopRect.width() + desktopRect.x() - 96;
        yMax = desktopRect.height() + desktopRect.y() - 95;
        xMin = desktopRect.x() - 54;
        yMin = desktopRect.y() - 16;
    }
    else if (scale == MEDIAN) {
        xMax = desktopRect.width() + desktopRect.x() - 127;
        yMax = desktopRect.height() + desktopRect.y() - 140;
        xMin = desktopRect.x() - 71;
        yMin = desktopRect.y() - 20;
    }
    else {
        xMax = desktopRect.width() + desktopRect.x() - 189;
        yMax = desktopRect.height() + desktopRect.y() - 230;
        xMin = desktopRect.x() - 108;
        yMin = desktopRect.y() - 30;
    }


    // 限制左上角位置
    newPos.setX(qMax(xMin, newPos.x()));
    newPos.setY(qMax(yMin, newPos.y()));

    // 限制右下角位置
    newPos.setX(qMin(xMax, newPos.x()));
    newPos.setY(qMin(yMax, newPos.y()));

    move(newPos);
}


// 鼠标事件. 区分处理拖拽 单击 双击
void MilesEdgeworth::mouseMoveEvent(QMouseEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    if (event->buttons() & Qt::LeftButton)
    {
        if (m_dragging)
        {
            //delta 相对偏移量
            QPoint delta = event->globalPosition().toPoint() - m_startPosition;
            //新位置：窗体原始位置+偏移量
            move(m_framePosition + delta);
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
        // 此段用于处理单击双击判断
        if (clickTimer->isActive()) {
            // 如果计时器正在运作,则说明这次press是第二次
            doubleClick = true;
            clickTimer->stop();         // 第二次press后关闭计时器
        }
    }
    QWidget::mousePressEvent(event);
}

void MilesEdgeworth::mouseReleaseEvent(QMouseEvent* event)
{
    if (type == MilesEdgeworth::BRIEFCASEIN) return;
    // 此段用于处理单双击判断
    if (event->button() == Qt::LeftButton && 
        event->globalPosition().toPoint() == m_startPosition) {
        // 如果release时与press时鼠标位置相同, 则不是拖拽, 预备进行单双击
        if (doubleClick) {
            // 若这是第二次点击的release, 则执行双击事件
            doubleClickEvent();
            doubleClick = false;
        }
        else {
            // 否则这是第一次点击的release, 开启计时器
            m_pressPos = event->position();
            clickTimer->start();
        }
    }
    // 此段用于处理拖拽移动
    m_dragging = false;

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

// 创建右键菜单
void MilesEdgeworth::createContextMenu()
{
    menu = new QMenu(this);
    sizeMenu = menu->addMenu("调整大小");
    languageMenu = menu->addMenu("语音语言");
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
            soundEffect->setVolume(0.2);
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
    tray->setVisible(true);
    tray->setIcon(QIcon(":/icon/favicon_bar.ico"));
    QMenu* menu = new QMenu(this);
    menu->addAction(actionExit);
    tray->setContextMenu(menu);
    tray->setToolTip(windowTitle());
    tray->show();

    connect(tray, &QSystemTrayIcon::activated, this, &MilesEdgeworth::on_trayActivated);
}


// 创建区分单双击用的计时器
void MilesEdgeworth::createClickTimer()
{
    clickTimer = new QTimer(this);
    clickTimer->setSingleShot(true);
    clickTimer->setInterval(300);   // 300ms内算双击
    connect(clickTimer, &QTimer::timeout, [=]() {
        // 执行单击事件
        singleClickEvent();
        });
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
            qDebug() << "切换单次动作随机数: " << num;
            num = (direction + 2 * num) % ONCE_NUM;
            qDebug() << "切换到: " << num;
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
    QPoint nowPos = frameGeometry().topLeft();
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
    move(nowPos + delta);
}

// 处理走路时的窗口移动
void MilesEdgeworth::walkMove()
{
    QPoint nowPos = frameGeometry().topLeft();
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
    move(nowPos + delta);
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
