#include <iostream>
#include <utility>

#include <QApplication>

#include "UpdateManager.h"
#include "begin.h"
#include "update_window.h"

int startProcessDownload(QApplication &app, QString &workDir);

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    QSharedMemory sharedMemApp("imister.kz-app_adskiller_v1");
    while(sharedMemApp.attach())
    {
        sharedMemApp.detach();
        QThread::msleep(500);
    }

    QSharedMemory sharedMemory("imister.kz-app_adskiller_v1_update");
    if(sharedMemory.attach())
    {
        return 1;
    }
    if(!sharedMemory.create(1))
    {
        return 1;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("AdsKiller update manager " + QString(APPVERSION));
    parser.addHelpOption();

    QCommandLineOption dirOption("dir", "AdsKiller work dir", "work directory");
    parser.addOption(dirOption);

    QCommandLineOption execOption("exec", "Run program after update", "program");
    parser.addOption(execOption);

    parser.process(a);

    QDir m;
    QString workDir = QApplication::applicationDirPath();

    if(!parser.isSet(dirOption))
    {
        if(!m.exists(workDir + QDir::separator() + "adskiller.exe"))
        {
            qDebug() << "Update manager require adskiller.exe";
            sharedMemory.detach();
            return 1;
        }
    }
    else
    {
        QDir dir(parser.value(dirOption));
        workDir = dir.absolutePath();
    }

    if(parser.isSet(execOption))
    {
        if(!m.exists(parser.value(execOption)))
        {
            qDebug() << "Program file is not exists";
            sharedMemory.detach();
            return 1;
        }
    }
    int exitCode = startProcessDownload(a, workDir);
    sharedMemory.detach();
    if(exitCode == 0 && parser.isSet(execOption))
        QProcess::startDetached(parser.value(execOption));
    return exitCode;
}

int startProcessDownload(QApplication &app, QString &workDir)
{
    int exitCode = 1;
    UpdateManager manager;
    UpdateWindow window;
    QThread downloadThread;

    manager.moveToThread(&downloadThread);
    QObject::connect(
        &downloadThread,
        &QThread::started,
        [&]()
        {
            QDir existDir(workDir);
            std::pair<QList<FetchResult>, int> result = manager.fetch();
            if(result.second < 0)
                return;
            result = manager.filter_by(existDir.path(), result.first);
            if(result.second < 0)
                return;
            if(manager.downloadAll(existDir.path(), result.first) < 0)
                return;
        });

    QTimer *progressUpdateTimer = new QTimer(&window);
    progressUpdateTimer->setInterval(100);
    progressUpdateTimer->setSingleShot(false);
    QObject::connect(
        progressUpdateTimer,
        &QTimer::timeout,
        [&]()
        {
            DownloadStatus downloads;
            QString lastErr;
            int status;
            lastErr = manager.getLastError(&status);
            if(status < 0)
            {
                window.setProgress(0, 0);
                window.setText("Download fails.\n" + lastErr);
                progressUpdateTimer->stop();
                window.delayPush(3000, [&]() { window.close(); });
                return;
            }
            double v1, v2;
            downloads = manager.downloadStatus();
            v1 = downloads.currentDownloadBytes;
            v1 /= std::max<quint64>(downloads.currentMaxDownloadBytes, 1u);
            if(manager.finishSuccess)
            {
                progressUpdateTimer->stop();
                if(downloads.maxDownloads == 0)
                    window.setText("Update is not required.");
                else
                    window.setText("Complete.\nClosing.");
                window.delayPush(1200, [&]() { window.close(); });
                v1 = 1.0F;
                v2 = 1.0F;
            }
            else
            {
                v2 = static_cast<double>(downloads.totalDownloadBytes) / std::max<quint64>(downloads.totalDownloadedBytes, 1u);
            }
            window.setProgress(static_cast<int>(v1 * 100), static_cast<int>(v2 * 100));
            if(!downloads.currentStatus.isEmpty())
                window.setText(QString("Download %1/%2: %3").arg(QString::number(downloads.downloadStep), QString::number(downloads.maxDownloads), downloads.currentStatus));
        });

    progressUpdateTimer->start();
    window.setProgress(0, 0);
    window.setText("Start download...");
    window.show();
    downloadThread.start();
    exitCode = app.exec();
    manager.stop();

    downloadThread.quit();
    downloadThread.wait();

    if(manager.finishSuccess)
        exitCode = 0;
    else
        exitCode |= 2;

    return exitCode;
}
