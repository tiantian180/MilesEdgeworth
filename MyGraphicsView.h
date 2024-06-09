#pragma once

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsPixmapItem>
#include <QMimeData>
#include <QFileInfo>
#include <QPixmapCache>

class MyGraphicsView  : public QGraphicsView
{
	Q_OBJECT

public:
	MyGraphicsView(QWidget *parent);
	~MyGraphicsView();
	void setPic(const QString& path);
	void showFullSize();
	void showFitSize();

	QSize getPicShowingSize() const;	// 获取当前图片展示的大小(不是原图大小)

protected:
	//注:设置了DragMode为ScrollHandDrag之后, 光标会自动随着拖拽改变, 不需要自己重写鼠标事件
	void wheelEvent(QWheelEvent* event) override;
	virtual void keyPressEvent(QKeyEvent* event) override;

signals:
	void loadingFailed();
	void loadingSucceed();

private:
	qreal m_scale = 1;
	double itemWidth;
	double itemHeight;
	QGraphicsScene* scene = nullptr;
};
