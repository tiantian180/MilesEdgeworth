#include "MilesEdgeworth/MilesEdgeworth.h"
#include <QApplication>
#include <QCoreApplication>
#include <QStringList>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);         // MilesEdgewort设置为Qt::Tool时, 如果不加这行会导致关闭图片查看器时自动退出程序
    MilesEdgeworth w;
    QObject::connect(&w, &MilesEdgeworth::exitProgram, &a, &QApplication::quit);    // 单击退出按钮时退出程序
    w.show();
    return a.exec();
}
