#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QSharedMemory>

#include "mainwindow.h"
#include "network.h"
#include "adbtrace.h"
#include "packages.h"

int main(int argc, char *argv[])
{
    QSharedMemory sharedMemUpdate("imister.kz-app_adskiller_v1_update");
    if(sharedMemUpdate.attach())
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

    QApplication app(argc, argv);
    // Set application Design
#ifndef WIN32
    QFile styleRes(":/resources/gravira-style");
    styleRes.open(QFile::ReadOnly | QFile::Text);
    QString styleSheet = styleRes.readAll();
    styleRes.close();
    app.setStyleSheet(styleSheet);
#endif

    MainWindow w;
    w.show();
    int exitCode = app.exec();
    sharedMem.detach();
    return exitCode;
}
