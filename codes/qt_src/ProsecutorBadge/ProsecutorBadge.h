#pragma once

#include <QWidget>
#include "ui_ProsecutorBadge.h"
#include <QPropertyAnimation>

const int FLYINGTIME = 1500;	// 徽章飞行时间

class ProsecutorBadge : public QWidget
{
	Q_OBJECT

public:
	ProsecutorBadge(QWidget *parent = nullptr);
	~ProsecutorBadge();

	virtual void mousePressEvent(QMouseEvent* event);
	void setScale(double new_scale);
	void moveAnimation(int direction);
	
	double scale = 1.5;

private:
	Ui::ProsecutorBadgeClass ui;
	QPropertyAnimation* animation;

signals:
	void badgeClicked();
};
