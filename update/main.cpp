#include <iostream>
#include <utility>

#include <QApplication>

#include "UpdateManager.h"
#include "begin.h"
#include "update_window.h"

int startProcessDownload(QApplication &app, QString &workDir, bool simulate);

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

    QCommandLineOption simulateOption("simulate", "Simulate update without downloading");
    parser.addOption(simulateOption);

    parser.process(a);

    QDir m;
    QString workDir = QApplication::applicationDirPath();

    if(!parser.isSet(dirOption))
    {
        if(!QFile::exists(workDir + QDir::separator() + "adskiller.exe"))
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
        if(!QFile::exists(parser.value(execOption)))
        {
            qDebug() << "Program file is not exists";
            sharedMemory.detach();
            return 1;
        }
    }
    int exitCode = startProcessDownload(a, workDir, parser.isSet(simulateOption));
    sharedMemory.detach();
    if(exitCode == 0 && parser.isSet(execOption))
        QProcess::startDetached(parser.value(execOption));
    return exitCode;
}

int startProcessDownload(QApplication &app, QString &workDir, bool simulate)
{
    int exitCode = 1;
    UpdateManager manager;
    manager.setSimulate(simulate);
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

    quint64 lastDownloadedBytes = 0;

    QObject::connect(
        progressUpdateTimer,
        &QTimer::timeout,
        [&, progressUpdateTimer]() // capture timer explicitly to prevent shadowing warnings
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
                window.delayPush(
                    3000,
                    [&]()
                    {
                        window.close();
                        app.quit();
                    });
                return;
            }
            double v1, v2;
            downloads = manager.downloadStatus();

            quint64 diffBytes = 0;
            if(downloads.totalDownloadedBytes > lastDownloadedBytes)
                diffBytes = downloads.totalDownloadedBytes - lastDownloadedBytes;
            quint64 speed = diffBytes * 10; // timer is 100ms, so x10 for bytes/sec
            lastDownloadedBytes = downloads.totalDownloadedBytes;

            auto formatBytes = [](quint64 bytes) -> QString
            {
                if(bytes > 1024 * 1024)
                    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
                if(bytes > 1024)
                    return QString::number(bytes / 1024.0, 'f', 1) + " KB";
                return QString::number(bytes) + " B";
            };

            v1 = downloads.currentDownloadBytes;
            v1 /= std::max<quint64>(downloads.currentMaxDownloadBytes, 1u);
            if(manager.finishSuccess)
            {
                progressUpdateTimer->stop();
                if(downloads.maxDownloads == 0)
                    window.setText("Update is not required.");
                else
                    window.setText("Complete.\nClosing.");
                window.delayPush(
                    1200,
                    [&app, &window]()
                    {
                        window.close();
                        app.quit();
                    });
                v1 = 1.0F;
                v2 = 1.0F;
            }
            else
            {
                v2 = static_cast<double>(downloads.totalDownloadedBytes) / std::max<quint64>(downloads.totalDownloadBytes, 1u);
            }
            window.setProgress(static_cast<int>(v1 * 100), static_cast<int>(v2 * 100));

            int blocks = static_cast<int>(v2 * 100);
            if(blocks > 100)
                blocks = 100;

            QString stats = QString("Скорость: <b>%1/s</b><br>Загружено: %2 / %3<br>Осталось: %4<br>Блоков: <b>%5 / 100</b>")
                                .arg(
                                    formatBytes(speed),
                                    formatBytes(downloads.totalDownloadedBytes),
                                    formatBytes(downloads.totalDownloadBytes),
                                    formatBytes(downloads.totalDownloadBytes > downloads.totalDownloadedBytes ? downloads.totalDownloadBytes - downloads.totalDownloadedBytes : 0),
                                    QString::number(blocks));
            window.setStats(stats);
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
