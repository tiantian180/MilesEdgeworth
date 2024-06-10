#include "ProsecutorBadge.h"

ProsecutorBadge::ProsecutorBadge(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	ui.badgeLabel->setScaledContents(true);
	animation = new QPropertyAnimation(this, "pos", this);
	animation->setDuration(FLYINGTIME); // 动画持续时间
	animation->setEasingCurve(QEasingCurve::OutSine);
}

ProsecutorBadge::~ProsecutorBadge()
{}

void ProsecutorBadge::mousePressEvent(QMouseEvent * event)
{
	emit badgeClicked();
}

void ProsecutorBadge::setScale(double new_scale)
{
	scale = new_scale;
	ui.badgeLabel->resize(12 * scale, 12 * scale);
}

void ProsecutorBadge::moveAnimation(int direction)
{
	animation->setStartValue(pos());       // 起始位置
	if (direction == 1) {
		animation->setEndValue(pos() + QPoint(-600 - 150 * scale, 0)); // 结束位置
	}
	else {
		animation->setEndValue(pos() + QPoint(600 + 150 * scale, 0)); // 结束位置
	}
	animation->start();
}
