#include "mainwindow.h"

#include "ElaApplication.h"
#include "ElaTheme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    eApp->init();
    eTheme->setThemeMode(ElaThemeType::Dark);

    MainWindow w;
    w.show();
    return a.exec();
}
