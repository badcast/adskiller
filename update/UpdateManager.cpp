#include "UpdateManager.h"

#ifndef NREMOTEADDR
#define NREMOTEADDR "https://adskiller.imister.tech/api"
#endif

constexpr int MaxTimeout = 10000;
constexpr int MaxDownloadAtemp = 4;
constexpr char URLFetch[] = NREMOTEADDR;

UpdateManager::UpdateManager(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
    m_manager->setTransferTimeout(5000);
    m_lastStatus = 0;
    m_forclyExit = 0;
    finishSuccess = 0;
    m_statusDownload = {};
}

std::pair<QList<FetchResult>, int> UpdateManager::fetch()
{
    QEventLoop loop;
    QNetworkReply *reply;
    QList<FetchResult> contents;
    m_manager->setTransferTimeout(MaxTimeout);
    reply = m_manager->get(QNetworkRequest(QUrl(QString(URLFetch) + "/cdn/update")));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [&]()
        {
            QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
            if(reply)
            {
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
                if(reply->error() != QNetworkReply::NoError)
                {
                    m_lastStatus = -1;
                }
                reply->deleteLater();
            }
            loop.quit();
        });
    loop.exec();
    if(m_lastStatus < 0)
        m_lastError = "No Internet connection.";
    return {std::move(contents), m_lastStatus};
}

std::pair<QList<FetchResult>, int> UpdateManager::filter_by(const QString &existsDir, const QList<FetchResult> &updates)
{
    if(m_simulate)
        return {updates, 0};
    QStringList files;
    QList<FetchResult> result;
    QDir dir(existsDir);
    while(1)
    {
        if(!dir.exists())
        {
            m_lastError = "Invalid Application directory found.";
            m_lastStatus = -1;
            break;
        }
        bool skip;
        QCryptographicHash hash(QCryptographicHash::Md5);
        files = getFilesEx(existsDir, GetFiles);
#ifdef WIN32
        for(QString &s : files)
        {
            s.replace("\\", "/");
        }
#endif

        QHash<QString, QString> hashes;
        for(QString &f : files)
        {
            QString file = existsDir + QDir::separator() + f;
            hashes[file] = getFileMD5Hash(file);
        }

        for(int x = 0; x < updates.size(); ++x)
        {
            skip = false;
            for(const QString &f : std::as_const(files))
            {
                if(updates[x].remoteLink == f)
                {
                    QString file = existsDir + QDir::separator() + f;
                    if(updates[x].md5hash == hashes[file])
                        skip = true;
                    break;
                }
            }
            if(!skip)
                result.append(updates[x]);
        }

        break;
    }
    return {result, m_lastStatus};
}

int UpdateManager::downloadAll(const QString &existsDir, const QList<FetchResult> &contents)
{
    if(contents.size() == 0)
    {
        finishSuccess = 1;
        m_lastStatus = 0;
    }
    else
    {
        QTemporaryDir tempDir;
        QDir dir;
        if(!tempDir.isValid())
        {
            m_lastError = "Failed make temp directory.";
            m_lastStatus = -1;
            return m_lastStatus;
        }
        dir.setPath(tempDir.path());
        m_statusDownload.maxDownloads = contents.size();
        m_statusDownload.totalDownloadBytes = std::accumulate(std::begin(contents), std::end(contents), 0, [](quint64 val, const FetchResult &t) { return val + t.bytes; });

        if(m_simulate)
        {
            for(int x = 0; x < contents.size() && !m_forclyExit; ++x)
            {
                const FetchResult *fetch = &contents[x];
                mutex.lock();
                m_statusDownload.downloadStep = x + 1;
                m_statusDownload.currentStatus = fetch->remoteLink.split("/", Qt::SkipEmptyParts).back();
                mutex.unlock();

                int chunks = 20;
                for(int c = 1; c <= chunks && !m_forclyExit; ++c)
                {
                    QThread::msleep(30); // 30ms * 20 = 600ms per file
                    QMutexLocker locker(&mutex);
                    quint64 chunkBytes = fetch->bytes / chunks;
                    if(c == chunks)
                        chunkBytes = fetch->bytes - (chunkBytes * (chunks - 1));
                    m_statusDownload.currentDownloadBytes = (fetch->bytes * c) / chunks;
                    m_statusDownload.currentMaxDownloadBytes = fetch->bytes;
                    m_statusDownload.totalDownloadedBytes += chunkBytes;
                }
            }
            mutex.lock();
            if(!m_forclyExit)
            {
                m_statusDownload.currentStatus = "Apply update";
                finishSuccess = true; // simulated success
            }
            else
            {
                m_statusDownload = {};
                m_lastStatus = -1;
                m_lastError = "Forcly exited";
            }
            mutex.unlock();
            return m_lastStatus;
        }

        int downloadAtempts = MaxDownloadAtemp;
        quint64 lastBytes;
        for(int x = 0; x < contents.size() && !m_forclyExit; ++x)
        {
            const FetchResult *fetch = &contents[x];
            QNetworkRequest request(QUrl(m_rootUrl + "/" + fetch->remoteLink));
            request.setTransferTimeout(MaxTimeout);
            QNetworkReply *reply = m_manager->get(request);
            QEventLoop loop;
            QFile file(tempDir.path() + QDir::separator() + fetch->remoteLink);

            // Make Sub dirs
            QFileInfo fileInfo(tempDir.path() + QDir::separator() + fetch->remoteLink);
            QDir().mkpath(fileInfo.path());

            mutex.lock();
            m_statusDownload.downloadStep = x + 1;
            m_statusDownload.currentStatus = fetch->remoteLink.split("/", Qt::SkipEmptyParts).back();
            mutex.unlock();

            lastBytes = 0;
            QObject::connect(
                reply,
                &QNetworkReply::downloadProgress,
                [&](qint64 bytesReceived, qint64 bytesTotal)
                {
                    QMutexLocker locker(&mutex);
                    m_statusDownload.currentDownloadBytes = bytesReceived;
                    m_statusDownload.currentMaxDownloadBytes = bytesTotal;
                    m_statusDownload.totalDownloadedBytes += bytesReceived - lastBytes;
                    lastBytes = bytesReceived;

                    if(m_forclyExit)
                    {
                        reply->close();
                        loop.quit();
                    }
                });

            QObject::connect(
                reply,
                &QNetworkReply::finished,
                [&]()
                {
                    if(reply)
                    {
                        if(!m_forclyExit)
                        {
                            if(reply->error() == QNetworkReply::NoError)
                            {
                                if(file.open(QFile::WriteOnly))
                                {
                                    file.write(reply->readAll());
                                    // Restore atemps
                                    downloadAtempts = MaxDownloadAtemp;
                                }
                                else
                                    // File write fails
                                    stop();
                            }
                            else
                            {
                                if(--downloadAtempts == 0)
                                {
                                    m_lastStatus = -1;
                                    m_lastError = "Network failed.";
                                    stop();
                                }
                                else // Retry download
                                {
                                    --x;
                                    QThread::msleep(500);
                                }
                            }
                        }
                        reply->deleteLater();
                    }
                    else
                    {
                        // Ignored ?: BUG
                        stop();
                    }
                    loop.quit();
                });
            loop.exec();
        }

        mutex.lock();
        if(m_lastStatus == 0 && !m_forclyExit)
        {
            m_statusDownload.currentStatus = "Apply update";
            finishSuccess = moveFilesTo(tempDir.path(), existsDir) == true;
        }
        else
        {
            m_statusDownload = {};
        }
        mutex.unlock();

        dir.removeRecursively();
    }
    return m_lastStatus;
}

void UpdateManager::stop()
{
    m_forclyExit = 1;
}

DownloadStatus UpdateManager::downloadStatus()
{
    QMutexLocker locker(&mutex);
    return m_statusDownload;
}

QString UpdateManager::getLastError(int *lastStatus)
{
    if(lastStatus)
        (*lastStatus) = m_lastStatus;
    return m_lastError;
}

QStringList UpdateManager::getFilesEx(const QString &path, int flags, QString _special)
{
    QDir dir(path);
    QStringList result;
    QStringList items = dir.entryList(QDir::Filter(flags & AllOF) | QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QString &item : std::as_const(items))
    {
        QString fullPath = dir.filePath(item);
        QFileInfo file(fullPath);
        if(file.isDir())
            result += getFilesEx(fullPath, flags, _special + item + QDir::separator());
        if((flags & GetFiles) && file.isFile() || (flags & GetDirs) && file.isDir())
            result.append(((flags & WriteFullpath) ? fullPath : (_special + item)));
    }
    return result;
}

QString UpdateManager::getFileMD5Hash(const QString &filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    std::byte _buffer[4096];
    QCryptographicHash hash(QCryptographicHash::Md5);
    QSpan<std::byte> spanbuffer = _buffer;
    while(!file.atEnd())
    {
        QByteArrayView buffer = file.readLineInto(spanbuffer);
        hash.addData(buffer);
    }

    file.close();
    return hash.result().toHex();
}

bool UpdateManager::moveFilesTo(const QString &sourcePath, const QString &destinationPath)
{
    QDir sourceDir(sourcePath);
    if(!sourceDir.exists())
    {
        return false;
    }

    QDir destinationDir(destinationPath);
    if(!destinationDir.exists())
    {
        if(!destinationDir.mkpath(destinationPath))
        {
            return false;
        }
    }

    // 1. Ensure all destination subdirectories exist
    QStringList dirEntries = getFilesEx(sourcePath, GetDirs);
    for(const QString &dirEntry : std::as_const(dirEntries))
    {
        destinationDir.mkpath(destinationDir.filePath(dirEntry));
    }

    // 2. Process each file: remove old or rename to .old if in use, then replace with new file
    QStringList fileEntries = getFilesEx(sourcePath, GetFiles);
    bool allSuccess = true;

    for(const QString &entry : std::as_const(fileEntries))
    {
        QString sourceFilePath = sourceDir.filePath(entry);
        QString destinationFilePath = destinationDir.filePath(entry);
        QString destBackFilePath = destinationFilePath + ".old";

        // Ensure parent directory exists for destination file
        QFileInfo destFileInfo(destinationFilePath);
        if(!destFileInfo.dir().exists())
        {
            destFileInfo.dir().mkpath(".");
        }

        // If destination file already exists
        if(QFile::exists(destinationFilePath))
        {
            // First try to remove it
            if(!QFile::remove(destinationFilePath))
            {
                // Deletion failed (file may be in use / process still running).
                // Rename it with .old suffix (e.g. adskiller.exe -> adskiller.exe.old)
                if(QFile::exists(destBackFilePath))
                {
                    if(!QFile::remove(destBackFilePath))
                    {
                        // If .old is also locked, rename it with a timestamp
                        QString tempOld = destBackFilePath + "." + QString::number(QDateTime::currentMSecsSinceEpoch());
                        QFile::rename(destBackFilePath, tempOld);
                    }
                }

                bool renamedToOld = false;
                int retryRename = 10;
                while(retryRename-- > 0)
                {
                    if(QFile::rename(destinationFilePath, destBackFilePath))
                    {
                        renamedToOld = true;
                        break;
                    }
                    QThread::msleep(100);
                }

                if(!renamedToOld)
                {
                    qWarning() << "Failed to rename in-use file to .old:" << destinationFilePath;
                }
            }
            else
            {
                // Deletion succeeded; clean up any existing .old file from previous runs
                if(QFile::exists(destBackFilePath))
                {
                    QFile::remove(destBackFilePath);
                }
            }
        }

        // 3. Move newly downloaded file to destination path
        bool replaced = false;
        int moveRetries = 10;
        while(moveRetries-- > 0)
        {
            // If destination file still exists somehow, try removing it
            if(QFile::exists(destinationFilePath))
            {
                QFile::remove(destinationFilePath);
            }

            if(QFile::rename(sourceFilePath, destinationFilePath))
            {
                replaced = true;
                break;
            }
            // Fallback for cross-filesystem / cross-drive moves
            if(QFile::copy(sourceFilePath, destinationFilePath))
            {
                QFile::remove(sourceFilePath);
                replaced = true;
                break;
            }
            QThread::msleep(100);
        }

        if(!replaced)
        {
            allSuccess = false;
            qWarning() << "Failed to replace file with updated version:" << destinationFilePath;
        }
    }

    return allSuccess;
}
