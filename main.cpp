#include "mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}