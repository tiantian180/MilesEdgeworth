/********************************************************************************
** Form generated from reading UI file 'PicViewer.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PICVIEWER_H
#define UI_PICVIEWER_H

#include <MyGraphicsView.h>
#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PicViewerClass
{
public:
    QAction *actOpen;
    QAction *actFull;
    QAction *actFit;
    QAction *actAdjust;
    QHBoxLayout *horizontalLayout;
    MyGraphicsView *graphicsView;

    void setupUi(QWidget *PicViewerClass)
    {
        if (PicViewerClass->objectName().isEmpty())
            PicViewerClass->setObjectName("PicViewerClass");
        PicViewerClass->resize(450, 270);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icon/prosbadge_icon.png"), QSize(), QIcon::Normal, QIcon::Off);
        PicViewerClass->setWindowIcon(icon);
        actOpen = new QAction(PicViewerClass);
        actOpen->setObjectName("actOpen");
        actOpen->setCheckable(false);
        actOpen->setChecked(false);
        actOpen->setMenuRole(QAction::NoRole);
        actFull = new QAction(PicViewerClass);
        actFull->setObjectName("actFull");
        actFull->setMenuRole(QAction::NoRole);
        actFit = new QAction(PicViewerClass);
        actFit->setObjectName("actFit");
        actFit->setMenuRole(QAction::NoRole);
        actAdjust = new QAction(PicViewerClass);
        actAdjust->setObjectName("actAdjust");
        actAdjust->setMenuRole(QAction::NoRole);
        horizontalLayout = new QHBoxLayout(PicViewerClass);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        graphicsView = new MyGraphicsView(PicViewerClass);
        graphicsView->setObjectName("graphicsView");
        graphicsView->setEnabled(true);
        graphicsView->setFrameShape(QFrame::NoFrame);
        graphicsView->setFrameShadow(QFrame::Plain);
        graphicsView->setLineWidth(0);

        horizontalLayout->addWidget(graphicsView);


        retranslateUi(PicViewerClass);

        QMetaObject::connectSlotsByName(PicViewerClass);
    } // setupUi

    void retranslateUi(QWidget *PicViewerClass)
    {
        PicViewerClass->setWindowTitle(QCoreApplication::translate("PicViewerClass", "PicViewer", nullptr));
        actOpen->setText(QCoreApplication::translate("PicViewerClass", "\346\267\273\345\212\240\345\233\276\347\211\207", nullptr));
#if QT_CONFIG(tooltip)
        actOpen->setToolTip(QCoreApplication::translate("PicViewerClass", "\351\200\211\346\213\251\345\233\276\347\211\207\346\226\207\344\273\266\346\267\273\345\212\240", nullptr));
#endif // QT_CONFIG(tooltip)
        actFull->setText(QCoreApplication::translate("PicViewerClass", "\346\230\276\347\244\272\345\216\237\345\233\276\345\244\247\345\260\217", nullptr));
#if QT_CONFIG(tooltip)
        actFull->setToolTip(QCoreApplication::translate("PicViewerClass", "\346\230\276\347\244\272\344\270\272\345\216\237\345\233\276\345\244\247\345\260\217", nullptr));
#endif // QT_CONFIG(tooltip)
        actFit->setText(QCoreApplication::translate("PicViewerClass", "\345\233\276\347\211\207\351\200\202\345\272\224\347\252\227\345\217\243", nullptr));
#if QT_CONFIG(tooltip)
        actFit->setToolTip(QCoreApplication::translate("PicViewerClass", "\346\230\276\347\244\272\344\270\272\351\200\202\345\272\224\345\261\217\345\271\225\345\244\247\345\260\217", nullptr));
#endif // QT_CONFIG(tooltip)
        actAdjust->setText(QCoreApplication::translate("PicViewerClass", "\347\252\227\345\217\243\351\200\202\345\272\224\345\233\276\347\211\207", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PicViewerClass: public Ui_PicViewerClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PICVIEWER_H
