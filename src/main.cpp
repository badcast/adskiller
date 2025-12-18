#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QSharedMemory>
#include <QBuffer>
#include <QDataStream>

#include "begin.h"
#include "mainwindow.h"
#include "network.h"

constexpr auto ShowCommandPipe = "adskiller_window_show";
constexpr auto HideCommandPipe = "adskiller_window_hide";

bool checkout();

int main(int argc, char **argv)
{
    int exitCode;
    QApplication app(argc, argv);
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach() || !checkout())
    {
        return EXIT_FAILURE;
    }
    QSharedMemory sharedMem("imister.kz-app_adskiller_v1");
    if(sharedMem.attach())
    {
        sharedMem.lock();
        int len = qMin<int>(strlen(ShowCommandPipe),sharedMem.size());
        memcpy(sharedMem.data(), ShowCommandPipe, len);
        sharedMem.unlock();
        sharedMem.detach();
        return EXIT_SUCCESS;
    }
    if(!sharedMem.create(128))
    {
        return EXIT_FAILURE;
    }

    MainWindow w;
    w.current = &w;
    w.delayPushLoop(100, [&sharedMem,&w]()->bool{
        if(!sharedMem.lock())
            return true;
        QString cmd = QLatin1String(reinterpret_cast<const char*>(sharedMem.constData()));
        if(cmd == ShowCommandPipe)
        {
            w.showNormal();
        }
        if(cmd == HideCommandPipe)
        {
            w.hide();
        }
        memset(sharedMem.data(), 0, 1);
        sharedMem.unlock();
        return true;
    });
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
