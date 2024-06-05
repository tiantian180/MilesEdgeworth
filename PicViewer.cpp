#include "PicViewer.h"

PicViewer::PicViewer(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	setWindowTitle("图片置顶查看器");
	setWindowIcon(QIcon(":/icon/prosbadge_icon.png"));
	setWindowFlags(Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setMouseTracking(true);		// 开启后, 无需按下左键就可触发mouseMoveEvent
	setAcceptDrops(true);

	createGraphicViewMenu();
	// 读取失败时展示读取失败的图片
	connect(ui.graphicsView, &MyGraphicsView::loadingFailed, [=]() {
		resize(450, 270);
		ui.graphicsView->setPic(":/loadingFail.png");
		});

	// 读取成功时, 调整窗口大小
	connect(ui.graphicsView, &MyGraphicsView::loadingSucceed, [=]() {
		// 如果不是特长或特宽图, 就让窗口适应图片; 否则, 改为在小窗口内显示原图大小
		if (ui.graphicsView->getPicShowingSize().width() > 180 && ui.graphicsView->getPicShowingSize().height() > 120) {
			resize(ui.graphicsView->getPicShowingSize());					// 窗口适应图片
		}
		else {
			ui.graphicsView->showFullSize();								// 显示原图大小
		}
		});
}

PicViewer::~PicViewer()
{}

void PicViewer::closeEvent(QCloseEvent* event)
{
	// 处理链表
	if (prev != nullptr) prev->next = next;
	if (next != nullptr) next->prev = prev;
	emit isClosing(prev == nullptr, next);
	this->deleteLater();
	QWidget::closeEvent(event);
}

void PicViewer::dragEnterEvent(QDragEnterEvent* event)
{
	// 拖入文件时(还未松手), 先判断是否可接受此文件
	QStringList acceptFileTypes;
	acceptFileTypes.append("jpg");
	acceptFileTypes.append("jpeg");
	acceptFileTypes.append("png");
	acceptFileTypes.append("bmp");
	acceptFileTypes.append("gif");
	acceptFileTypes.append("pbm");
	acceptFileTypes.append("pgm");
	acceptFileTypes.append("ppm");
	acceptFileTypes.append("xbm");
	acceptFileTypes.append("xpm");

	if (event->mimeData()->hasUrls() && event->mimeData()->urls().count() == 1) {
		// 对象有URL列表，并且只拖入了一个文件, 则:
		QFileInfo file(event->mimeData()->urls().at(0).toLocalFile());
		if (acceptFileTypes.contains(file.suffix().toLower())) {
			// 拖入的文件后缀在accepetFileTypes中, 则
			event->acceptProposedAction();
		}
	}
}

void PicViewer::dropEvent(QDropEvent* event)
{
	// drop文件时
	QFileInfo file(event->mimeData()->urls().at(0).toLocalFile());	// 获取文件信息
	resize(450, 450);												// 让读取的图片显示时尺寸不超过450*450
	ui.graphicsView->setPic(file.absoluteFilePath());
	setWindowTitle(file.fileName());
}


void PicViewer::createGraphicViewMenu()
{
	menu = new QMenu(this);
	//menu->addAction(ui.actOpen);
	menu->addAction(ui.actFit);
	menu->addAction(ui.actFull);
	menu->addAction(ui.actAdjust);
	opacityMenu = menu->addMenu("窗口不透明度");

	opacityGroup = new QActionGroup(opacityMenu);
	opacityGroup->setExclusive(true);  // 设置为单选

	hundredOpacity = opacityMenu->addAction("100%");
	hundredOpacity->setCheckable(true);			// 设置action可选中
	hundredOpacity->setChecked(true);			// 默认选中
	opacityGroup->addAction(hundredOpacity);	// 加入action组

	ninetyOpacity = opacityMenu->addAction("90%");
	ninetyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(ninetyOpacity);		// 加入action组

	eightyOpacity = opacityMenu->addAction("80%");
	eightyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(eightyOpacity);		// 加入action组

	seventyOpacity = opacityMenu->addAction("70%");
	seventyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(seventyOpacity);	// 加入action组

	sixtyOpacity = opacityMenu->addAction("60%");
	sixtyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(sixtyOpacity);		// 加入action组

	fiftyOpacity = opacityMenu->addAction("50%");
	fiftyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(fiftyOpacity);		// 加入action组

	fortyOpacity = opacityMenu->addAction("40%");
	fortyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(fortyOpacity);		// 加入action组

	thirtyOpacity = opacityMenu->addAction("30%");
	thirtyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(thirtyOpacity);		// 加入action组

	twentyOpacity = opacityMenu->addAction("20%");
	twentyOpacity->setCheckable(true);			// 设置action可选中
	opacityGroup->addAction(twentyOpacity);		// 加入action组

	tenOpacity = opacityMenu->addAction("10%");
	tenOpacity->setCheckable(true);				// 设置action可选中
	opacityGroup->addAction(tenOpacity);		// 加入action组

	// 右键显示菜单
	ui.graphicsView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.graphicsView, &QWidget::customContextMenuRequested, [=]() {
		menu->exec(QCursor::pos());
		});

	// 显示为原图大小
	connect(ui.actFull, &QAction::triggered, ui.graphicsView, &MyGraphicsView::showFullSize);
	// 图片适应窗口
	connect(ui.actFit, &QAction::triggered, ui.graphicsView, &MyGraphicsView::showFitSize);
	// 窗口适应图片
	connect(ui.actAdjust, &QAction::triggered, [=]() {
		resize(ui.graphicsView->getPicShowingSize());
		});
	connect(hundredOpacity, &QAction::triggered, [=]() {
		if (hundredOpacity->isChecked())
			setWindowOpacity(1.0);
		});
	connect(ninetyOpacity, &QAction::triggered, [=]() {
		if (ninetyOpacity->isChecked())
			setWindowOpacity(0.9);
		});
	connect(eightyOpacity, &QAction::triggered, [=]() {
		if (eightyOpacity->isChecked())
			setWindowOpacity(0.8);
		});
	connect(seventyOpacity, &QAction::triggered, [=]() {
		if (seventyOpacity->isChecked())
			setWindowOpacity(0.7);
		});
	connect(sixtyOpacity, &QAction::triggered, [=]() {
		if (sixtyOpacity->isChecked())
			setWindowOpacity(0.6);
		});
	connect(fiftyOpacity, &QAction::triggered, [=]() {
		if (fiftyOpacity->isChecked())
			setWindowOpacity(0.5);
		});
	connect(fortyOpacity, &QAction::triggered, [=]() {
		if (fortyOpacity->isChecked())
			setWindowOpacity(0.4);
		});
	connect(thirtyOpacity, &QAction::triggered, [=]() {
		if (thirtyOpacity->isChecked())
			setWindowOpacity(0.3);
		});
	connect(twentyOpacity, &QAction::triggered, [=]() {
		if (twentyOpacity->isChecked())
			setWindowOpacity(0.2);
		});
	connect(tenOpacity, &QAction::triggered, [=]() {
		if (tenOpacity->isChecked())
			setWindowOpacity(0.1);
		});
}
