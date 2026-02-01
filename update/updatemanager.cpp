#include "UpdateManager.h"

constexpr int MaxTimeout = 10000;
constexpr int MaxDownloadAtemp = 4;
constexpr char URLFetch[] = "https://adskill.imister.kz/cdn/update";

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
    reply = m_manager->get(QNetworkRequest(QUrl(URLFetch)));
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
            for(const QString &f : files)
            {
                if(updates[x].remoteLink == f)
                {
                    QString file = existsDir + QDir::separator() + f;
                    if(updates[x].md5hash == hashes[file])
                        skip = true;
                    else
                        dir.remove(file);
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
            QStringList subDirs = fetch->remoteLink.split("/");
            for(int y = 0; y < subDirs.size() - 1; ++y)
            {
                dir.mkdir(tempDir.path() + QDir::separator() + subDirs[y]);
            }

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

    QStringList entries = getFilesEx(sourcePath, GetDirs);
    QTemporaryDir _lostTemps;
    for(const QString &entry : std::as_const(entries))
    {
        destinationDir.mkdir(destinationDir.filePath(entry));
        destinationDir.mkdir(_lostTemps.filePath(entry));
    }

    entries = getFilesEx(sourcePath, GetFiles);
    for(const QString &entry : std::as_const(entries))
    {
        QString sourceFilePath = sourceDir.filePath(entry);
        QString destinationFilePath = destinationDir.filePath(entry);
        QString destBackupFile = entry + "_old";
        QString destBackFilePath = destinationDir.filePath(destBackupFile);
        if(destinationDir.exists(destBackFilePath) && !QFile::remove(destBackFilePath))
        {
            QFile::rename(destBackFilePath, _lostTemps.filePath(destBackupFile));
        }
        QFile::rename(destinationFilePath, destinationDir.filePath(destBackFilePath));

        int states = 10;
        while(!QFile::rename(sourceFilePath, destinationFilePath) && states--)
        {
            QThread::msleep(100);
        }
    }
    return true;
}
