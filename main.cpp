#include "mainwindow.h"
#include "Logger.h"

#include "ElaApplication.h"
#include "ElaTheme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Logger::init("logs");
    LOG("程序启动");

    eApp->init();
    eTheme->setThemeMode(ElaThemeType::Dark);

    MainWindow w;
    w.showMaximized();

    const int result = a.exec();
    LOG("程序退出，返回码: %d", result);
    Logger::close();
    return result;
}
