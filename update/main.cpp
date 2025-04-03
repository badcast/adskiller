#include <iostream>
#include <utility>

#include <QApplication>
#include <QList>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSharedMemory>
#include <QProcess>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QCryptographicHash>

#include "update_window.h"

constexpr int MaxDownloadAtemp = 4;

struct FetchResult
{
    QString remoteLink;
    QString md5hash;
    quint64 bytes;
};

struct StatusDownload
{
    quint64 currentDownload;
    quint64 currentMaxDownload;
    quint64 totalCurrentDownloaded;
    quint64 totalDownloaded;
    int downloadStep;
    int maxDownloads;
    QString file;
};

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    UpdateManager(QObject *parent = nullptr) : QObject(parent)
    {
        m_manager = new QNetworkAccessManager(this);
        m_lastStatus = 0;
        m_forclyExit = 0;
        finishSuccess = 0;
        m_statusDownload = {};
    }

    std::pair<QList<FetchResult>,int> fetch()
    {
        constexpr char URLFetch[] = "https://adskill.imister.kz/v1/update";
        QEventLoop loop;
        QNetworkReply * reply;
        QList<FetchResult> contents;
        reply = m_manager->get(QNetworkRequest(QUrl(URLFetch)));
        connect(reply, &QNetworkReply::finished, this, [&]()
                {
                    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
                    if(reply){
                        while(reply->error() == QNetworkReply::NoError)
                        {
                            QByteArray responce = reply->readAll();
                            QJsonDocument jres = QJsonDocument::fromJson(responce);
                            QJsonArray array;
                            if(jres.isNull())
                            {
                                break;
                            }
                            m_rootUrl = jres["root"].toString();
                            m_version = jres["version"].toString();
                            m_totalBytes = jres["totalBytes"].toVariant().toULongLong();
                            array = jres["files"].toArray();
                            FetchResult fr;
                            for(int x = 0; x < array.size(); ++x)
                            {
                                QJsonObject obj = array[x].toObject();
                                // TODO: check "hashType" for multiple hash support
                                fr.md5hash = obj["hash"].toString();
                                fr.remoteLink = obj["url"].toString();
                                fr.bytes = obj["bytes"].toVariant().toULongLong();
                                contents.append(fr);
                            }
                            m_lastStatus = 0;
                            break;
                        }
                        reply->deleteLater();
                    }
                    loop.quit();
                });
        loop.exec();
        if(m_lastStatus < 0)
            m_lastError = "No Internet connection.";
        return {std::move(contents),m_lastStatus};
    }

    std::pair<QList<FetchResult>,int> filter_by(const QString &existsDir, const QList<FetchResult> & updates)
    {
        QStringList files;
        QList<FetchResult> result;
        QDir dir(existsDir);
        while(1)
        {
            if(!dir.exists())
            {
                m_lastError = "Invlid Application directory found.";
                m_lastStatus = -1;
                break;
            }
            bool skip;
            QCryptographicHash hash(QCryptographicHash::Md5);
            files = recursivellyFiles(existsDir);
            for(QString & s : files)
            {
                s.remove(0, existsDir.size()+1);
            }

            for(int x = 0; x < updates.size(); ++x)
            {
                skip = false;
                for(QString & f : files)
                {
                    if(updates[x].remoteLink == f)
                    {
                        if(updates[x].md5hash == calcMD5(existsDir + "/" + f))
                        {
                            skip = true;
                        }
                        else
                        {
                            dir.remove(existsDir + "/" +f);
                        }
                        break;
                    }
                }
                if(!skip)
                    result.append(updates[x]);
            }

            break;
        }
        return {result,m_lastStatus};
    }

    int downloadAll(const QString &existsDir, const QList<FetchResult> &contents)
    {
        if(contents.size() == 0)
        {
            finishSuccess = 1;
            m_lastStatus = 0;
        }
        else
        {
            m_statusDownload.maxDownloads = contents.size();
            QString tempDir("/tmp/test");
            QDir dir(tempDir);

            if((dir.exists() && !dir.removeRecursively()) || !dir.mkdir(tempDir))
            {
                m_lastError = "Failed make temp directory.";
                m_lastStatus = -1;
                return m_lastStatus;
            }

            m_statusDownload.totalDownloaded = std::accumulate(std::begin(contents), std::end(contents), 0, [](quint64 val, auto & t){
                return val + t.bytes;
            });

            for(int x = 0; x < contents.size() && !m_forclyExit; ++x)
            {
                const FetchResult* fetch = &contents[x];
                QNetworkRequest request(QUrl(m_rootUrl + "/" + fetch->remoteLink));
                QNetworkReply * reply = m_manager->get(request);
                QEventLoop loop;
                QFile file(tempDir + QDir::separator() + fetch->remoteLink);

                // Make Sub dirs
                QStringList subDirs = fetch->remoteLink.split("/");
                for(int y = 0; y < subDirs.size()-1; ++y)
                {
                    dir.mkdir(tempDir + QDir::separator() + subDirs[y]);
                }

                mutex.lock();
                m_statusDownload.downloadStep = x+1;
                m_statusDownload.file = fetch->remoteLink.split("/", Qt::SkipEmptyParts).back();
                mutex.unlock();

                quint64 lastBytes = 0;
                connect(reply, &QNetworkReply::downloadProgress, [&](qint64 bytesReceived, qint64 bytesTotal)
                        {
                            QMutexLocker locker(&mutex);
                            m_statusDownload.currentDownload = bytesReceived;
                            m_statusDownload.currentMaxDownload = bytesTotal;
                            m_statusDownload.totalCurrentDownloaded += bytesReceived - lastBytes;
                            lastBytes = bytesReceived;

                            if(m_forclyExit)
                            {
                                reply->close();
                                loop.quit();
                            }
                        });

                connect(reply, &QNetworkReply::finished, [&](){
                    if(reply)
                    {
                        if(reply->error() == QNetworkReply::NoError)
                        {
                            if(file.open(QFile::WriteOnly))
                            {
                                file.write(reply->readAll());
                            }
                            else
                            {

                            }
                        }
                        reply->deleteLater();
                    }
                    loop.quit();
                });
                loop.exec();
            }

            if(m_lastStatus == 0 && !m_forclyExit)
            {
                moveContents(tempDir, existsDir);
                finishSuccess = 1;
            }

            dir.removeRecursively();
        }
        return m_lastStatus;
    }

    void stop()
    {
        m_forclyExit = 1;
        finishSuccess = 0;
    }

    StatusDownload downloadStatus()
    {
        QMutexLocker locker(&mutex);
        return m_statusDownload;
    }

    QString getLastError(int * lastStatus = nullptr)
    {
        if(lastStatus)
            (*lastStatus) = m_lastStatus;
        return m_lastError;
    }

    int finishSuccess;

private:
    QStringList recursivellyFiles(const QString& path)
    {
        QDir dir(path);
        QStringList result;
        QStringList items = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &item : items) {
            QString fullPath = dir.filePath(item);
            QFileInfo file(fullPath);

            if (file.isDir()) {
                result += recursivellyFiles(fullPath);
            } else {
#ifdef WIN32
                fullPath.replace("\\", "/");
#endif
                result.append(fullPath);
            }
        }
        return result;
    }

    QString calcMD5(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Md5);
        while (!file.atEnd()) {
            QByteArray line = file.read(4096);
            hash.addData(line);
        }

        file.close();
        return hash.result().toHex();
    }
    bool moveContents(const QString &sourcePath, const QString &destinationPath) {
        QDir sourceDir(sourcePath);

        if (!sourceDir.exists()) {
            return false;
        }

        QDir destinationDir(destinationPath);
        if (!destinationDir.exists()) {
            if (!destinationDir.mkpath(destinationPath)) {
                return false;
            }
        }

        QStringList entries = sourceDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);

        foreach (const QString &entry, entries) {
            QString sourceFilePath = sourceDir.filePath(entry);
            QString destinationFilePath = destinationDir.filePath(entry);
            QFile::rename(sourceFilePath, destinationFilePath);
        }

        return true;
    }


private:
    QNetworkAccessManager * m_manager;
    QString m_rootUrl;
    QString m_version;
    quint64 m_totalBytes;
    int m_forclyExit;
    QMutex mutex;
    StatusDownload m_statusDownload;
    int m_lastStatus;
    QString m_lastError;
};

int main(int argc, char** argv)
{
    int exitCode = 1;
    QSharedMemory sharedMemApp("imister.kz-app_adskiller_v1");
    while(sharedMemApp.attach())
    {
        sharedMemApp.detach();
        QThread::msleep(500);
    }

    QSharedMemory sharedMem("imister.kz-app_adskiller_v1_update");
    if(sharedMem.attach())
    {
        return 1;
    }
    if(!sharedMem.create(1))
    {
        return 1;
    }

    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("AdsKiller update manager " + QString(APPVERSION));
    parser.addHelpOption();

    QCommandLineOption dirOption("dir", "AdsKiller work dir", "work directory");
    parser.addOption(dirOption);

    parser.process(a);

    QString workDir = QCoreApplication::applicationDirPath();
    QDir m;
    m.mkdir(workDir);

    if(!parser.isSet(dirOption))
    {
        if(!m.exists(workDir + QDir::separator() + "adskiller.exe"))
        {
            qDebug() << "Update manager require adskiller.exe";
            sharedMem.detach();
            return 1;
        }
    }
    else
    {
        workDir = parser.value(dirOption);
    }

    UpdateManager manager;
    update_window w;
    QThread downloadThread;
    QObject::connect(&downloadThread, &QThread::started, [&](){
        std::pair<QList<FetchResult>,int> result = manager.fetch();
        if(result.second < 0)
            return;
        result = manager.filter_by(workDir, result.first);
        if(result.second < 0)
            return;
        if(manager.downloadAll(workDir, result.first) < 0)
            return;
    });

    QTimer * progressUpdateTimer = new QTimer(&w);
    progressUpdateTimer->setInterval(100);
    progressUpdateTimer->setSingleShot(false);
    QObject::connect(progressUpdateTimer, &QTimer::timeout, [&]()
                     {
                         StatusDownload downloads;
                         int status;
                         QString lastErr = manager.getLastError(&status);
                         if(status)
                         {
                             w.setText((status < 0 ? "Download fails.\n" : "") + lastErr);
                         }
                         double v1,v2;
                         downloads = manager.downloadStatus();
                         v1 = downloads.currentDownload;
                         v1 /= std::max<quint64>(downloads.currentMaxDownload,1u);
                         if(manager.finishSuccess)
                         {
                             progressUpdateTimer->stop();
                             if(downloads.maxDownloads == 0)
                                 w.setText("Update is not required.");
                             else
                                 w.setText("Complete.\nClosing.");
                             w.delayPush(1200, [&](){
                                 w.close();
                             });
                             v1 = 1.0F;
                             v2 = 1.0F;
                         }
                         else
                             v2 = static_cast<double>(downloads.totalCurrentDownloaded) / std::max<quint64>(downloads.totalDownloaded,1u);
                         w.setProgress(static_cast<int>(v1*100), static_cast<int>(v2*100));
                         if(!downloads.file.isEmpty())
                             w.setText(QString("Download %1/%2: %3").arg(QString::number(downloads.downloadStep)).arg(QString::number(downloads.maxDownloads)).arg(downloads.file));
                     });

    progressUpdateTimer->start();
    w.setProgress(0,0);
    w.setText("Start download...");
    w.show();
    downloadThread.start();

    exitCode = a.exec();
    manager.stop();

    downloadThread.quit();
    downloadThread.wait();

    sharedMem.detach();
    return exitCode;
}

#include "main.moc"
