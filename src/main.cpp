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

bool checkout();

int main(int argc, char *argv[])
{
    int exitCode;
    QApplication app(argc, argv);
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach() || !checkout())
    {
        return 1;
    }
    QSharedMemory sharedMem("imister.kz-app_adskiller_v1");
    if(sharedMem.attach())
    {
        QBuffer buffer(&sharedMem);
        if(!buffer.open(QBuffer::WriteOnly))
            qDebug() << "fail open buffer sharedmem";
        QDataStream outStream(&buffer);
        outStream << QString("show_window");
        buffer.close();
        sharedMem.detach();
        return 1;
    }
    if(!sharedMem.create(1024))
    {
        return 1;
    }

    MainWindow w;
    w.delayPushLoop(100, [&sharedMem,&w]()->bool{

        QBuffer buffer(&sharedMem);
        if(!buffer.open(QBuffer::ReadOnly))
            qDebug() << "Fail init shared mem buffer";

        QDataStream inStream(&buffer);


        QString cmd;
        inStream >> cmd;

        if(!cmd.isEmpty())
        {
            int xx = 0;
        }
        if(cmd == "show_window")
        {
            w.show();
        }
        buffer.close();
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
