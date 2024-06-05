/********************************************************************************
** Form generated from reading UI file 'ProsecutorBadge.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PROSECUTORBADGE_H
#define UI_PROSECUTORBADGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ProsecutorBadgeClass
{
public:
    QLabel *badgeLabel;

    void setupUi(QWidget *ProsecutorBadgeClass)
    {
        if (ProsecutorBadgeClass->objectName().isEmpty())
            ProsecutorBadgeClass->setObjectName("ProsecutorBadgeClass");
        ProsecutorBadgeClass->resize(94, 94);
        ProsecutorBadgeClass->setMaximumSize(QSize(94, 94));
        badgeLabel = new QLabel(ProsecutorBadgeClass);
        badgeLabel->setObjectName("badgeLabel");
        badgeLabel->setGeometry(QRect(0, 0, 18, 18));
        badgeLabel->setPixmap(QPixmap(QString::fromUtf8(":/prosbadge.png")));
        badgeLabel->setScaledContents(true);

        retranslateUi(ProsecutorBadgeClass);

        QMetaObject::connectSlotsByName(ProsecutorBadgeClass);
    } // setupUi

    void retranslateUi(QWidget *ProsecutorBadgeClass)
    {
        ProsecutorBadgeClass->setWindowTitle(QCoreApplication::translate("ProsecutorBadgeClass", "ProsecutorBadge", nullptr));
        badgeLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class ProsecutorBadgeClass: public Ui_ProsecutorBadgeClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PROSECUTORBADGE_H
