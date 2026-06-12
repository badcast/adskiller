#include "Services.h"
#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>

struct FileExplorerService::PrivateRes
{
    AdbFileIO *fileIO = nullptr;
};

FileExplorerService::FileExplorerService(QObject *parent) : Service(DeviceConnectType::ADB, parent), _priv(new PrivateRes)
{
}

FileExplorerService::~FileExplorerService()
{
    if(_priv->fileIO)
    {
        delete _priv->fileIO;
    }
    delete _priv;
}

void FileExplorerService::stop()
{
    mIsRunning = false;
    mSuccessState = true;
}

bool FileExplorerService::start()
{
    if(!mAdbDevice.isEmpty())
    {
        mIsRunning = true;
        mSuccessState = false;

        if(_priv->fileIO)
        {
            delete _priv->fileIO;
        }
        _priv->fileIO = new AdbFileIO(mAdbDevice.devId);

        // Try connecting or assume it works
        _priv->fileIO->connect(mAdbDevice.devId);

        // Notify that we've "started" the service (meaning we're connected to the device)
        mIsRunning = false;
        mSuccessState = true;
        return true;
    }
    return false;
}

void FileExplorerService::service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok)
{
    // Local mock service doesn't need remote server verification usually
}

QVariantList FileExplorerService::listFiles(const QString &path)
{
    QVariantList list;
    if(!_priv->fileIO)
        return list;

    QStringList files = _priv->fileIO->getFiles(path, true);
    for(const QString &jsonStr : files)
    {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if(!doc.isNull() && doc.isObject())
        {
            list.append(doc.object().toVariantMap());
        }
    }
    return list;
}

bool FileExplorerService::deleteFile(const QString &path)
{
    if(!_priv->fileIO)
        return false;
    return _priv->fileIO->deleteFile(path);
}

bool FileExplorerService::pushFile(const QString &localPath, const QString &remotePath)
{
    if(!_priv->fileIO)
        return false;
    // Strip file:// if provided by QML FileDialog
    QString cleanLocal = localPath;
    if(cleanLocal.startsWith("file://"))
    {
        cleanLocal = cleanLocal.mid(7);
    }
    return _priv->fileIO->push(cleanLocal, remotePath);
}

bool FileExplorerService::pullFile(const QString &remotePath, const QString &localPath)
{
    if(!_priv->fileIO)
        return false;
    // Strip file:// if provided by QML FileDialog
    QString cleanLocal = localPath;
    if(cleanLocal.startsWith("file://"))
    {
        cleanLocal = cleanLocal.mid(7);
    }
    return _priv->fileIO->pull(remotePath, cleanLocal);
}
