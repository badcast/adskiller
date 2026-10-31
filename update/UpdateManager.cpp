#include "UpdateManager.h"

#include <numeric>

#ifndef NREMOTEADDR
#define NREMOTEADDR "http://localhost:8000/api"
#endif

constexpr int MaxTimeout = 10000;
constexpr int MaxDownloadAtemp = 4;
constexpr quint64 MaxSingleFileBytes = 512ULL * 1024ULL * 1024ULL; // 512 MB safety cap
constexpr char URLFetch[] = NREMOTEADDR;

UpdateManager::UpdateManager(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
    m_manager->setTransferTimeout(5000);
    m_lastStatus = 0;
    m_forclyExit = 0;
    finishSuccess = 0;
    m_statusDownload = {};
    m_totalBytes = 0;
}

void UpdateManager::setError(const QString &error, int status)
{
    QMutexLocker locker(&mutex);
    m_lastError = error;
    m_lastStatus = status;
}

bool UpdateManager::isSafeRelativePath(const QString &baseDir, const QString &relativePath, QString *resolvedPath)
{
    if(relativePath.isEmpty() || relativePath.contains(QChar('\0')) || relativePath.contains(QLatin1Char(':')))
    {
        return false;
    }

    const QString normRel = QDir::fromNativeSeparators(relativePath.trimmed());
    if(normRel.isEmpty() || normRel.startsWith(QLatin1Char('/')))
    {
        return false;
    }

    const QStringList segments = normRel.split(QLatin1Char('/'));
    for(const QString &seg : segments)
    {
        if(seg.isEmpty() || seg == QLatin1String(".") || seg == QLatin1String(".."))
        {
            return false;
        }
    }

    const QDir base(baseDir);
    QString cleanBase = QDir::cleanPath(base.absolutePath());
    const QString candidate = QDir::cleanPath(base.absoluteFilePath(normRel));

    if(!cleanBase.endsWith(QLatin1Char('/')))
    {
        cleanBase += QLatin1Char('/');
    }

    if(!candidate.startsWith(cleanBase))
    {
        return false;
    }

    if(resolvedPath)
    {
        *resolvedPath = candidate;
    }
    return true;
}

QCryptographicHash::Algorithm UpdateManager::parseHashAlgorithm(const QString &hashType, const QString &hashHex)
{
    const QString normalized = hashType.trimmed().toLower();
    if(normalized == QLatin1String("sha256") || normalized == QLatin1String("sha-256"))
        return QCryptographicHash::Sha256;
    if(normalized == QLatin1String("sha512") || normalized == QLatin1String("sha-512"))
        return QCryptographicHash::Sha512;
    if(normalized == QLatin1String("sha384") || normalized == QLatin1String("sha-384"))
        return QCryptographicHash::Sha384;
    if(normalized == QLatin1String("sha1") || normalized == QLatin1String("sha-1"))
        return QCryptographicHash::Sha1;
    if(normalized == QLatin1String("md5"))
        return QCryptographicHash::Md5;

    switch(hashHex.trimmed().length())
    {
        case 64:
            return QCryptographicHash::Sha256;
        case 96:
            return QCryptographicHash::Sha384;
        case 128:
            return QCryptographicHash::Sha512;
        case 40:
            return QCryptographicHash::Sha1;
        default:
            return QCryptographicHash::Md5;
    }
}

std::pair<QList<FetchResult>, int> UpdateManager::fetch()
{
    QEventLoop loop;
    QList<FetchResult> contents;
    m_manager->setTransferTimeout(MaxTimeout);
    const QUrl fetchEndpoint(QString(URLFetch) + "/cdn/update");
    QNetworkReply *reply = m_manager->get(QNetworkRequest(fetchEndpoint));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [&]()
        {
            if(reply)
            {
                if(reply->error() == QNetworkReply::NoError)
                {
                    const QByteArray responce = reply->readAll();
                    const QJsonDocument jres = QJsonDocument::fromJson(responce);
                    if(!jres.isObject())
                    {
                        setError("Invalid update manifest format.");
                    }
                    else
                    {
                        const QJsonObject rootObj = jres.object();
                        m_rootUrl = rootObj["root"].toString().trimmed();
                        while(m_rootUrl.endsWith(QLatin1Char('/')))
                        {
                            m_rootUrl.chop(1);
                        }
                        m_version = rootObj["version"].toString();
                        m_totalBytes = rootObj["totalBytes"].toVariant().toULongLong();
                        const QString defaultHashType = rootObj["hashType"].toString();

                        const QUrl parsedRoot(m_rootUrl);
                        const bool schemeAllowed = (parsedRoot.scheme() == QLatin1String("https")) || (parsedRoot.scheme() == QLatin1String("http") && fetchEndpoint.scheme() == QLatin1String("http") && parsedRoot.host().compare(fetchEndpoint.host(), Qt::CaseInsensitive) == 0);

                        if(!parsedRoot.isValid() || parsedRoot.host().isEmpty() || !schemeAllowed)
                        {
                            setError("Untrusted or invalid update server URL.");
                        }
                        else
                        {
                            const QJsonArray array = rootObj["files"].toArray();
                            bool manifestValid = true;
                            for(int x = 0; x < array.size(); ++x)
                            {
                                const QJsonObject obj = array[x].toObject();
                                FetchResult fr;
                                fr.md5hash = obj["hash"].toString().trimmed();
                                fr.remoteLink = QDir::fromNativeSeparators(obj["url"].toString().trimmed());
                                fr.bytes = obj["bytes"].toVariant().toULongLong();
                                fr.hashAlgo = parseHashAlgorithm(obj["hashType"].toString(defaultHashType), fr.md5hash);

                                if(!isSafeRelativePath(QDir::tempPath(), fr.remoteLink) || fr.bytes > MaxSingleFileBytes)
                                {
                                    setError("Unsafe file entry in update manifest.");
                                    manifestValid = false;
                                    contents.clear();
                                    break;
                                }
                                contents.append(fr);
                            }

                            if(manifestValid)
                            {
                                QMutexLocker locker(&mutex);
                                m_lastStatus = 0;
                                m_lastError.clear();
                            }
                        }
                    }
                }
                else
                {
                    setError("No Internet connection.");
                }
                reply->deleteLater();
            }
            loop.quit();
        });
    loop.exec();

    int status = 0;
    getLastError(&status);
    return {std::move(contents), status};
}

std::pair<QList<FetchResult>, int> UpdateManager::filter_by(const QString &existsDir, const QList<FetchResult> &updates)
{
    if(m_simulate)
        return {updates, 0};

    QList<FetchResult> result;
    QDir dir(existsDir);
    if(!dir.exists())
    {
        setError("Invalid Application directory found.");
        return {result, -1};
    }

    for(int x = 0; x < updates.size(); ++x)
    {
        QString localFilePath;
        if(!isSafeRelativePath(existsDir, updates[x].remoteLink, &localFilePath))
        {
            setError("Unsafe file path detected.");
            return {{}, -1};
        }

        if(QFile::exists(localFilePath) && !updates[x].md5hash.isEmpty())
        {
            const QString localHash = getFileHash(localFilePath, updates[x].hashAlgo);
            if(!localHash.isEmpty() && localHash.compare(updates[x].md5hash, Qt::CaseInsensitive) == 0)
            {
                continue;
            }
        }
        result.append(updates[x]);
    }

    int status = 0;
    getLastError(&status);
    return {result, status};
}

int UpdateManager::downloadAll(const QString &existsDir, const QList<FetchResult> &contents)
{
    if(contents.isEmpty())
    {
        finishSuccess = 1;
        QMutexLocker locker(&mutex);
        m_lastStatus = 0;
        return 0;
    }

    QTemporaryDir tempDir;
    QDir dir;
    if(!tempDir.isValid())
    {
        setError("Failed make temp directory.");
        return -1;
    }
    dir.setPath(tempDir.path());

    {
        QMutexLocker locker(&mutex);
        m_statusDownload.maxDownloads = contents.size();
        m_statusDownload.totalDownloadBytes = std::accumulate(std::begin(contents), std::end(contents), quint64(0), [](quint64 val, const FetchResult &t) { return val + t.bytes; });
    }

    if(m_simulate)
    {
        for(int x = 0; x < contents.size() && !m_forclyExit.load(); ++x)
        {
            const FetchResult *fetch = &contents[x];
            {
                QMutexLocker locker(&mutex);
                m_statusDownload.downloadStep = x + 1;
                const QStringList parts = fetch->remoteLink.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                m_statusDownload.currentStatus = parts.isEmpty() ? fetch->remoteLink : parts.back();
            }

            int chunks = 20;
            for(int c = 1; c <= chunks && !m_forclyExit.load(); ++c)
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
        if(!m_forclyExit.load())
        {
            QMutexLocker locker(&mutex);
            m_statusDownload.currentStatus = "Apply update";
            finishSuccess = 1; // simulated success
        }
        else
        {
            {
                QMutexLocker locker(&mutex);
                m_statusDownload = {};
            }
            setError("Forcly exited");
        }
        int status = 0;
        getLastError(&status);
        return status;
    }

    int downloadAtempts = MaxDownloadAtemp;
    for(int x = 0; x < contents.size() && !m_forclyExit.load(); ++x)
    {
        const FetchResult *fetch = &contents[x];
        QString targetFilePath;
        if(!isSafeRelativePath(tempDir.path(), fetch->remoteLink, &targetFilePath))
        {
            setError("Unsafe destination path during download.");
            stop();
            break;
        }

        // Make Sub dirs safely inside tempDir
        const QFileInfo fileInfo(targetFilePath);
        if(!QDir().mkpath(fileInfo.path()))
        {
            setError("Failed to create temporary directory structure.");
            stop();
            break;
        }

        QFile file(targetFilePath);
        if(!file.open(QFile::WriteOnly | QFile::Truncate))
        {
            setError("Failed to open temporary file for writing.");
            stop();
            break;
        }

        {
            QMutexLocker locker(&mutex);
            m_statusDownload.downloadStep = x + 1;
            const QStringList parts = fetch->remoteLink.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            m_statusDownload.currentStatus = parts.isEmpty() ? fetch->remoteLink : parts.back();
        }

        QNetworkRequest request(QUrl(m_rootUrl + "/" + fetch->remoteLink));
        request.setTransferTimeout(MaxTimeout);
        QNetworkReply *reply = m_manager->get(request);
        QEventLoop loop;

        quint64 attemptBytesCounted = 0;
        bool writeErrorOccurred = false;
        bool sizeLimitExceeded = false;

        QObject::connect(
            reply,
            &QNetworkReply::readyRead,
            this,
            [&]()
            {
                if(m_forclyExit.load() || !file.isOpen())
                    return;

                const QByteArray chunk = reply->readAll();
                if(chunk.isEmpty())
                    return;

                const quint64 newSize = static_cast<quint64>(file.size()) + static_cast<quint64>(chunk.size());
                const quint64 maxAllowed = (fetch->bytes > 0) ? fetch->bytes : MaxSingleFileBytes;
                if(newSize > maxAllowed)
                {
                    sizeLimitExceeded = true;
                    reply->abort();
                    return;
                }

                if(file.write(chunk) != chunk.size())
                {
                    writeErrorOccurred = true;
                    reply->abort();
                }
            });

        QObject::connect(
            reply,
            &QNetworkReply::downloadProgress,
            this,
            [&](qint64 bytesReceived, qint64 bytesTotal)
            {
                QMutexLocker locker(&mutex);
                const quint64 received = static_cast<quint64>(std::max<qint64>(0, bytesReceived));
                const quint64 total = static_cast<quint64>(std::max<qint64>(0, bytesTotal));
                m_statusDownload.currentDownloadBytes = received;
                m_statusDownload.currentMaxDownloadBytes = total;
                if(received >= attemptBytesCounted)
                {
                    m_statusDownload.totalDownloadedBytes += (received - attemptBytesCounted);
                    attemptBytesCounted = received;
                }

                if(m_forclyExit.load())
                {
                    reply->abort();
                    loop.quit();
                }
            });

        QObject::connect(
            reply,
            &QNetworkReply::finished,
            this,
            [&]()
            {
                if(reply)
                {
                    if(!m_forclyExit.load())
                    {
                        auto rollbackProgressAndFile = [&]()
                        {
                            if(file.isOpen())
                                file.close();
                            QFile::remove(targetFilePath);
                            QMutexLocker locker(&mutex);
                            if(m_statusDownload.totalDownloadedBytes >= attemptBytesCounted)
                                m_statusDownload.totalDownloadedBytes -= attemptBytesCounted;
                            else
                                m_statusDownload.totalDownloadedBytes = 0;
                        };

                        if(!writeErrorOccurred && !sizeLimitExceeded && reply->error() == QNetworkReply::NoError)
                        {
                            const QByteArray remaining = reply->readAll();
                            if(!remaining.isEmpty())
                            {
                                const quint64 newSize = static_cast<quint64>(file.size()) + static_cast<quint64>(remaining.size());
                                const quint64 maxAllowed = (fetch->bytes > 0) ? fetch->bytes : MaxSingleFileBytes;
                                if(newSize > maxAllowed)
                                {
                                    sizeLimitExceeded = true;
                                }
                                else if(file.write(remaining) != remaining.size())
                                {
                                    writeErrorOccurred = true;
                                }
                            }
                            file.flush();
                            file.close();
                        }
                        else if(file.isOpen())
                        {
                            file.close();
                        }

                        if(writeErrorOccurred)
                        {
                            rollbackProgressAndFile();
                            setError("Failed to write downloaded file to disk.");
                            stop();
                        }
                        else if(sizeLimitExceeded)
                        {
                            rollbackProgressAndFile();
                            setError("Downloaded file exceeded expected size.");
                            stop();
                        }
                        else if(reply->error() == QNetworkReply::NoError)
                        {
                            const quint64 actualSize = static_cast<quint64>(QFileInfo(targetFilePath).size());
                            const bool sizeOk = (fetch->bytes == 0 || actualSize == fetch->bytes);
                            const bool hashOk = fetch->md5hash.isEmpty() || (getFileHash(targetFilePath, fetch->hashAlgo).compare(fetch->md5hash, Qt::CaseInsensitive) == 0);

                            if(sizeOk && hashOk)
                            {
                                downloadAtempts = MaxDownloadAtemp;
                            }
                            else
                            {
                                rollbackProgressAndFile();
                                if(--downloadAtempts == 0)
                                {
                                    setError("File integrity verification failed.");
                                    stop();
                                }
                                else
                                {
                                    --x;
                                    QThread::msleep(500);
                                }
                            }
                        }
                        else
                        {
                            rollbackProgressAndFile();
                            if(--downloadAtempts == 0)
                            {
                                setError("Network failed.");
                                stop();
                            }
                            else // Retry download
                            {
                                --x;
                                QThread::msleep(500);
                            }
                        }
                    }
                    else
                    {
                        if(file.isOpen())
                            file.close();
                        QFile::remove(targetFilePath);
                    }
                    reply->deleteLater();
                }
                else
                {
                    setError("Internal network reply error.");
                    stop();
                }
                loop.quit();
            });
        loop.exec();
    }

    int status = 0;
    getLastError(&status);

    if(status == 0 && !m_forclyExit.load())
    {
        {
            QMutexLocker locker(&mutex);
            m_statusDownload.currentStatus = "Apply update";
        }
        if(moveFilesTo(tempDir.path(), existsDir))
        {
            finishSuccess = 1;
        }
        else
        {
            finishSuccess = 0;
            setError("Failed to replace application files.");
        }
    }
    else
    {
        QMutexLocker locker(&mutex);
        m_statusDownload = {};
        if(m_lastStatus == 0)
        {
            m_lastStatus = -1;
            if(m_lastError.isEmpty())
                m_lastError = "Update interrupted.";
        }
    }

    dir.removeRecursively();
    getLastError(&status);
    return status;
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
    QMutexLocker locker(&mutex);
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
    return getFileHash(filePath, QCryptographicHash::Md5);
}

QString UpdateManager::getFileHash(const QString &filePath, QCryptographicHash::Algorithm algo)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }

    QCryptographicHash hash(algo);
    if(!hash.addData(&file))
    {
        file.close();
        return QString();
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

    // 1. Ensure all destination subdirectories exist safely inside destinationDir
    QStringList dirEntries = getFilesEx(sourcePath, GetDirs);
    for(const QString &dirEntry : std::as_const(dirEntries))
    {
        QString safeSubDir;
        if(!isSafeRelativePath(destinationPath, dirEntry, &safeSubDir))
        {
            return false;
        }
        destinationDir.mkpath(safeSubDir);
    }

    // 2. Process each file: remove old or rename to .old if in use, then replace with new file
    QStringList fileEntries = getFilesEx(sourcePath, GetFiles);
    bool allSuccess = true;

    for(const QString &entry : std::as_const(fileEntries))
    {
        QString sourceFilePath;
        QString destinationFilePath;
        if(!isSafeRelativePath(sourcePath, entry, &sourceFilePath) || !isSafeRelativePath(destinationPath, entry, &destinationFilePath))
        {
            allSuccess = false;
            continue;
        }
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
