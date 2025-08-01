#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QSharedMemory>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"

bool checkout();

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach() || !checkout())
    {
        return 1;
    }
    QSharedMemory sharedMem("imister.kz-app_adskiller_v1");
    if(sharedMem.attach())
    {
        QMessageBox::warning(nullptr, "Внимание", "Приложение уже запущено.");
        return 1;
    }
    if(!sharedMem.create(1))
    {
        return 1;
    }
    MainWindow w;
    int exitCode;
    w.app = &app;
    w.show();
    exitCode = app.exec();
    sharedMem.detach();
    return exitCode;
}

bool checkout()
{
    QDir qdir;
    QString adbfile = AdbExecutableFilename();
    if(!qdir.exists(adbfile))
    {
        qDebug() << "Adb not found";
        return false;
    }
    return true;
}
