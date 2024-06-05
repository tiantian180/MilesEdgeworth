/********************************************************************************
** Form generated from reading UI file 'MilesEdgeworth.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MILESEDGEWORTH_H
#define UI_MILESEDGEWORTH_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MilesEdgeworthClass
{
public:
    QLabel *petLabel;

    void setupUi(QWidget *MilesEdgeworthClass)
    {
        if (MilesEdgeworthClass->objectName().isEmpty())
            MilesEdgeworthClass->setObjectName("MilesEdgeworthClass");
        MilesEdgeworthClass->resize(300, 300);
        MilesEdgeworthClass->setMinimumSize(QSize(300, 300));
        MilesEdgeworthClass->setMaximumSize(QSize(300, 300));
        petLabel = new QLabel(MilesEdgeworthClass);
        petLabel->setObjectName("petLabel");
        petLabel->setGeometry(QRect(0, 0, 100, 100));

        retranslateUi(MilesEdgeworthClass);

        QMetaObject::connectSlotsByName(MilesEdgeworthClass);
    } // setupUi

    void retranslateUi(QWidget *MilesEdgeworthClass)
    {
        MilesEdgeworthClass->setWindowTitle(QCoreApplication::translate("MilesEdgeworthClass", "MilesEdgeworth", nullptr));
        petLabel->setText(QCoreApplication::translate("MilesEdgeworthClass", "Edgeworth", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MilesEdgeworthClass: public Ui_MilesEdgeworthClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MILESEDGEWORTH_H
