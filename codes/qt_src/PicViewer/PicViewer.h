#pragma once

#include <QWidget>
#include <QMenu>
#include <QActionGroup>

#include "MyGraphicsView.h"
#include "ui_PicViewer.h"


class PicViewer : public QWidget
{
	Q_OBJECT

public:
	PicViewer(QWidget *parent = nullptr);
	~PicViewer();
	
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;

	void createGraphicViewMenu();
	PicViewer* next = nullptr;
	PicViewer* prev = nullptr;

signals:
	void isClosing(bool prevIsNull, PicViewer* nextPtr);

private:
	Ui::PicViewerClass ui;
	QMenu* menu;
	QMenu* opacityMenu;
	QActionGroup* opacityGroup;
	QAction* zeroOpacity;
	QAction* tenOpacity;
	QAction* twentyOpacity;
	QAction* thirtyOpacity;
	QAction* fortyOpacity;
	QAction* fiftyOpacity;
	QAction* sixtyOpacity;
	QAction* seventyOpacity;
	QAction* eightyOpacity;
	QAction* ninetyOpacity;
	QAction* hundredOpacity;
};
