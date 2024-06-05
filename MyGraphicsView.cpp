#include "MyGraphicsView.h"

MyGraphicsView::MyGraphicsView(QWidget *parent)
	: QGraphicsView(parent)
{
	setDragMode(QGraphicsView::ScrollHandDrag);				  // 设置拖拽模式为调整滚动条
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse); // 变换锚点设为鼠标位置
	setResizeAnchor(QGraphicsView::AnchorUnderMouse);		  // 大小变换锚点设为鼠标位置
	setStyleSheet("padding: 0px; border: 0px;");		      //无边框
	setMouseTracking(true);//跟踪鼠标位置
	setCursor(Qt::OpenHandCursor);
	setAcceptDrops(false);
	resize(450, 270);
	setPic(":/dropPic.png");
}

MyGraphicsView::~MyGraphicsView()
{
	QPixmapCache::clear();			// 防止随着浏览图片数量增加占用内存不断升高
}

//由路径显示图片
void MyGraphicsView::setPic(const QString& path)
{
	if (!path.isNull()) {
		QPixmapCache::clear();		// 防止随着浏览图片数量增加占用内存不断升高
		if (scene != nullptr) {
			scene->clear();			// 移除并delete scene中的item
			delete scene;
			scene = nullptr;
		}
		scale(1 / m_scale, 1 / m_scale);
		m_scale = 1;
		QPixmap pixmap(path);
		if (pixmap.isNull()) {
			emit loadingFailed();
		}
		else {
			scene = new QGraphicsScene(this);
			setScene(scene);
			QGraphicsPixmapItem* item = scene->addPixmap(pixmap);
			// 记录原图尺寸
			itemWidth = item->boundingRect().width();
			itemHeight = item->boundingRect().height();
			showFitSize();
			emit loadingSucceed();
		}
	}
}


void MyGraphicsView::showFullSize()
{
	resetTransform();
	m_scale = 1;
	centerOn(itemWidth / 2, itemHeight / 2);
}


void MyGraphicsView::showFitSize()
{
	qreal zoomWidth = this->size().width() / itemWidth;
	qreal zoomHeight = this->size().height() / itemHeight;
	qreal scaleFit = zoomWidth < zoomHeight ? zoomWidth : zoomHeight;
	scale(scaleFit/m_scale, scaleFit/m_scale);
	m_scale = scaleFit;
	update();
}

QSize MyGraphicsView::getPicShowingSize() const
{
	return QSize(m_scale * itemWidth, m_scale * itemHeight);
}


// 重写滚轮事件
void MyGraphicsView::wheelEvent(QWheelEvent* event)
{
	if (Qt::ALT & event->modifiers())
	{// 按住ctrl时滚动滚轮,则放大缩小图片
		m_scale = this->transform().m11();
		int delta = event->angleDelta().x();	// 按alt键时, x和y是反的
		if (delta > 0)	
		{// 向上滚
			if (m_scale < 50)
			{// 如果还没有达到放大的上限,则可以放大
				m_scale *= 1.2;
				this->scale(1.2, 1.2);
			}
		}
		else			
		{// 向下滚
			if (m_scale > 0.01)
			{// 如果还没有达到缩小的下限,则可以缩小
				m_scale /= 1.2;
				this->scale(1 / 1.2, 1 / 1.2);
			}
		}
		update();
	}
	else  
	{//没有按ctrl时,交给父类处理
		QGraphicsView::wheelEvent(event);
	}
}

void MyGraphicsView::keyPressEvent(QKeyEvent* event)
{
	if (event->modifiers() & Qt::CTRL) { 
		if (event->key() == Qt::Key_Equal) { 
			// ctrl和+ 放大
			qDebug() << "fangda";
			if (m_scale < 50)
			{// 如果还没有达到放大的上限,则可以放大
				m_scale *= 1.2;
				this->scale(1.2, 1.2);
			}
		}
		else if (event->key() == Qt::Key_Minus) { 
			// ctrl和- 缩小
			qDebug() << "缩小";
			if (m_scale > 0.01)
			{// 如果还没有达到缩小的下限,则可以缩小
				m_scale /= 1.2;
				this->scale(1 / 1.2, 1 / 1.2);
			}
		}
	}
}

