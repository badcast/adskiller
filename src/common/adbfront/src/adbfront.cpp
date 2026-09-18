#include <algorithm>
#include <limits>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <QStringList>
#include <QTemporaryFile>

#include "adbcmds.h"
#include "adbfront.h"

QString AdbExecutableFilename()
{
    QString adbFile;
#ifdef WIN32
    adbFile = QCoreApplication::applicationDirPath() + "/adb/";
#elif __linux__
    adbFile = "/usr/bin/";
#endif
    adbFile += AdbFilename;
    return adbFile;
}

std::pair<bool, QString> adb_send_cmd(int &exitCode, const QStringList &arguments)
{
    QProcess process;
    QString retval;
    process.start(AdbExecutableFilename(), arguments);
    if(!process.waitForFinished(ExecWaitTime))
    {
        qDebug() << "ADB Failed";
        process.kill();
        process.waitForFinished(ExecWaitTime);
        return {false, {}};
    }
    retval = process.readAllStandardOutput();
    exitCode = process.exitCode();
#ifdef WIN32
    retval.replace("\r\n", "\n");
#endif
    return {exitCode == 0, retval};
}

std::pair<bool, QString> adb_send_cmd(const QStringList &arguments)
{
    int code;
    return adb_send_cmd(code, arguments);
}

bool operator==(const AdbDevice &lhs, const AdbDevice &rhs)
{
    return Adb::deviceHash(lhs) == Adb::deviceHash(rhs);
}

Adb::Adb(QObject *parent) : QObject(parent), deviceWatchTimer(nullptr)
{
}

Adb::~Adb()
{
    adb_send_cmd(QStringList() << "kill-server");
}

QList<AdbDevice> Adb::getDevices()
{
    QList<AdbDevice> devices;
    QStringList lines;
    std::pair<bool, QString> reply = adb_send_cmd(QStringList() << "devices");
    if(reply.first)
    {
        lines = reply.second.split("\n", Qt::SkipEmptyParts);
        for(const QString &line : std::as_const(lines))
        {
            if(line.startsWith("List of devices"))
                continue;
            devices.append(getDevice(line.split('\t').first()));
        }
    }
    return devices;
}

uint Adb::deviceHash(const AdbDevice &device)
{
    uint retval;
    QString _compare;
    _compare += device.devId;
    _compare += device.model;
    _compare += device.vendor;
    retval = qHash(_compare);
    return retval;
}

AdbConStatus Adb::status() const
{
    return deviceStatus(device.devId);
}

bool Adb::isConnected()
{
    return status() == DEVICE;
}

std::pair<bool, std::unique_ptr<AdbShell>> Adb::runShell()
{
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(QString {});
    bool connectStatus = shell->connect(device.devId);
    return std::make_pair(connectStatus, std::move(shell));
}

void Adb::connectFirst()
{
    QList<AdbDevice> devs = std::move(getDevices());
    if(!devs.isEmpty())
        connect(devs.front().devId);
}

void Adb::connect(const QString &devId)
{
    QList<AdbDevice> devs = std::move(getDevices());
    QList<AdbDevice>::ConstIterator iter;
    iter = std::find_if(devs.cbegin(), devs.cend(), [&devId](const AdbDevice &dev) { return dev.devId == devId; });
    if(iter == devs.cend())
    {
        qDebug() << "No allowed device";
        return;
    }
    if(isConnected())
        disconnect();
    device = *iter;
    emit onDeviceChanged(device, AdbConState::Add);
    deviceWatchTimer = new QTimer(this);
    deviceWatchTimer->start(100);
    QObject::connect(deviceWatchTimer, &QTimer::timeout, this, &Adb::onDeviceWatch);
}

QList<PackageIO> Adb::getPackages(const QString &deviceSerial)
{
    QList<PackageIO> packages;
    QStringList packageList, disableList;
    QSet<QString> optimizedList;
    std::pair<bool, QString> reply;
    std::pair<bool, QString> reply2dis;
    int pkgSeprIndx = 0;
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(deviceSerial);
    if(!shell->isConnect())
    {
        qDebug() << "Device is not connected";
        return {};
    }
    reply = shell->commandQueueWait(
        QStringList() << "pm" << "list" << "packages"
                      << "--user" << "0" << "-3");
    reply2dis = shell->commandQueueWait(
        QStringList() << "pm" << "list" << "packages"
                      << "--user" << "0" << "-3" << "-d");
    if(reply.first && reply2dis.first)
    {
        reply.second.remove('\t');
        packageList = reply.second.split('\n', Qt::SkipEmptyParts);
        disableList = reply2dis.second.split('\n', Qt::SkipEmptyParts);
        if(packageList.count() > 0)
            pkgSeprIndx = packageList.first().indexOf(':') + 1;

        std::function<QString(QString &)> removeSeperators = [pkgSeprIndx](QString &pkg) { return pkg.remove(0, pkgSeprIndx); };
        std::transform(std::begin(disableList), std::end(disableList), std::begin(disableList), removeSeperators);
        std::transform(std::begin(packageList), std::end(packageList), std::begin(packageList), removeSeperators);
        optimizedList = std::move(QSet<QString>(disableList.begin(), disableList.end()));

        for(QString &item : packageList)
        {
            packages.append(PackageIO {});
            packages.last().packageName = item;
            packages.last().applicationName = "Unknown";
            packages.last().disabled = optimizedList.contains(item);
        }
    }
    return packages;
}

void Adb::killPackages(const QString &deviceSerial, const QList<PackageIO> &packages, int &successCount)
{
    successCount = 0;
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(deviceSerial);
    if(!shell->isConnect())
        return;
    for(const PackageIO &package : packages)
    {
        if(shell->stopPackage(package.packageName))
            successCount++;
    }
}

bool Adb::uninstallPackages(const QString &deviceSerial, const QStringList &packages, int &successCount)
{
    successCount = 0;
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(deviceSerial);
    if(!shell->isConnect())
        return false;
    for(const QString &package : packages)
    {
        if(shell->uninstallPackage(package))
            successCount++;
    }
    return true;
}

bool Adb::disablePackages(const QString &deviceSerial, const QStringList &packages, int &successCount)
{
    successCount = 0;
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(deviceSerial);
    if(!shell->isConnect())
        return false;
    for(const QString &package : packages)
    {
        if(shell->disablePackage(package))
            successCount++;
    }
    return true;
}

bool Adb::enablePackages(const QString &deviceSerial, const QStringList &packages, int &successCount)
{
    successCount = 0;
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(deviceSerial);
    if(!shell->isConnect())
        return false;
    for(const QString &package : packages)
    {
        if(shell->enablePackage(package))
            successCount++;
    }
    return true;
}

void Adb::onDeviceWatch()
{
    AdbConStatus deviceStatus = status();
    if(deviceStatus != AdbConStatus::DEVICE)
    {
        AdbDevice old = device;
        cachedDevices.removeOne(device);
        device = {};
        emit onDeviceChanged(old, AdbConState::Removed);
        deviceWatchTimer->stop();
        deviceWatchTimer->deleteLater();
        deviceWatchTimer = nullptr;
    }
}

AdbConStatus Adb::deviceStatus(const QString &deviceId)
{
    AdbConStatus retval = UNKNOWN;
    QString res;
    if(!deviceId.isEmpty())
    {
        std::pair<bool, QString> reply = adb_send_cmd(QStringList() << "-s" << deviceId << "get-state");
        if(reply.first)
        {
            res = reply.second.trimmed();
            if(res == "unauthorized" || res == "unathourized")
                retval = UNAUTH;
            else if(res == "device")
                retval = DEVICE;
        }
        else
        {
            qDebug() << "Failed try get-state";
        }
    }
    return retval;
}

AdbConStatus Adb::deviceStatus(const AdbDevice &device)
{
    return Adb::deviceStatus(device.devId);
}

void Adb::disconnect()
{
    if(!isConnected())
    {
        qDebug() << "No connected";
        return;
    }
    AdbDevice old = device;
    device = {};
    emit onDeviceChanged(old, AdbConState::Removed);
    if(deviceWatchTimer)
    {
        deviceWatchTimer->stop();
        delete deviceWatchTimer;
        deviceWatchTimer = nullptr;
    }
}

void Adb::startServer()
{
    adb_send_cmd(QStringList() << "start-server");
}

void Adb::killServer()
{
    adb_send_cmd(QStringList() << "kill-server");
}

AdbDevice Adb::getDevice(const QString &deviceSerial)
{
    constexpr auto NoAllowString = "(No allow)";

    AdbShell shell;
    AdbDevice dev;
    dev.devId = deviceSerial;
    if(shell.connect(deviceSerial))
    {
        dev.model = shell.getprop(PropProductModel);
        dev.vendor = shell.getprop(PropProductManufacturer);
        dev.marketingName = shell.getprop(PropProductMarketingName);
        dev.displayName = dev.marketingName.isEmpty() ? dev.vendor + " " + dev.model : dev.marketingName;
    }
    else
    {
        dev.model = NoAllowString;
        dev.vendor = NoAllowString;
        dev.marketingName = NoAllowString;
        dev.displayName = NoAllowString;
    }
    return dev;
}

bool AdbDevice::isEmpty() const
{
    return devId.isEmpty() && model.isEmpty() && displayName.isEmpty() && vendor.isEmpty();
}

AdbFileIO::AdbFileIO(const QString &deviceId) : AdbShell(deviceId)
{
}

AdbFileIO::AdbFileIO(AdbFileIO &&other) noexcept : AdbShell(std::move(other))
{
}

AdbFileIO &AdbFileIO::operator=(AdbFileIO &&other) noexcept
{
    AdbShell::operator=(std::move(other));
    return *this;
}

bool AdbFileIO::exists(const QString &filePath)
{
    if(!isConnect() || filePath.isEmpty())
        return false;
    auto res = commandQueueWaits("[ -e \"" + filePath + "\" ] && echo __OK_EXISTS__");
    return res.first && res.second.contains("__OK_EXISTS__");
}

bool AdbFileIO::deleteFile(const QString &filePath)
{
    if(!isConnect() || filePath.isEmpty() || filePath == "/" || filePath == "/sdcard" || filePath == "/storage")
        return false;
    commandQueueWaits("rm -rf \"" + filePath + "\"");
    return !exists(filePath);
}

bool AdbFileIO::makeDir(const QString &dirPath)
{
    if(!isConnect() || dirPath.isEmpty())
        return false;
    commandQueueWaits("mkdir -p \"" + dirPath + "\"");
    return exists(dirPath);
}


QByteArray AdbFileIO::read(const QString &filePath)
{
    QString dev = deviceId();
    if(dev.isEmpty() || filePath.isEmpty())
        return {};
    QTemporaryFile tempFile;
    if(tempFile.open())
    {
        QString tempPath = tempFile.fileName();
        tempFile.close();
        if(pullFile(filePath, tempPath))
        {
            QFile file(tempPath);
            if(file.open(QIODevice::ReadOnly))
            {
                QByteArray data = file.readAll();
                file.close();
                QFile::remove(tempPath);
                return data;
            }
        }
        QFile::remove(tempPath);
    }
    return {};
}

bool AdbFileIO::write(const QString &filePath, const QByteArray &buffer)
{
    QString dev = deviceId();
    if(dev.isEmpty() || filePath.isEmpty())
        return false;
    QTemporaryFile tempFile;
    if(tempFile.open())
    {
        tempFile.write(buffer);
        tempFile.flush();
        QString tempPath = tempFile.fileName();
        tempFile.close();
        bool ok = pushFile(tempPath, filePath);
        QFile::remove(tempPath);
        return ok;
    }
    return false;
}

QStringList AdbFileIO::getFiles(const QString &dirPath, bool includeDirs)
{
    QStringList result;
    if(!isConnect() || dirPath.isEmpty())
        return result;

    QString cleanDir = dirPath;
    if(!cleanDir.endsWith('/'))
        cleanDir += '/';

    auto res = commandQueueWaits("ls -1p \"" + cleanDir + "\" 2>/dev/null");
    if(!res.first)
        return result;

    QStringList lines = res.second.split('\n', Qt::SkipEmptyParts);
    for(QString &line : lines)
    {
        line = line.trimmed();
        if(line.isEmpty() || line == "./" || line == "../" || line == "." || line == "..")
            continue;
        bool isDirectory = line.endsWith('/');
        if(!includeDirs && isDirectory)
            continue;
        result.append(cleanDir + line);
    }
    return result;
}

QList<AdbFileInfo> AdbFileIO::getFileList(const QString &dirPath)
{
    QList<AdbFileInfo> list;
    if(!isConnect() || dirPath.isEmpty())
        return list;

    QString cleanDir = dirPath;
    if(!cleanDir.endsWith('/'))
        cleanDir += '/';

    // Query detailed directory listing
    auto res = commandQueueWaits("ls -la \"" + cleanDir + "\" 2>/dev/null");
    if(res.first && !res.second.trimmed().isEmpty())
    {
        QStringList lines = res.second.split('\n', Qt::SkipEmptyParts);
        for(const QString &rawLine : lines)
        {
            QString line = rawLine.trimmed();
            if(line.isEmpty() || line.startsWith("total ", Qt::CaseInsensitive))
                continue;

            QStringList tokens = line.split(' ', Qt::SkipEmptyParts);
            if(tokens.size() < 4)
                continue;

            QString perms = tokens[0];
            bool isDir = perms.startsWith('d');
            bool isLink = perms.startsWith('l');

            // Find filename after time token (HH:MM or YYYY)
            int nameStartIdx = -1;
            for(int i = 3; i < tokens.size(); ++i)
            {
                if(tokens[i].contains(':') && i + 1 < tokens.size())
                {
                    nameStartIdx = i + 1;
                    break;
                }
            }

            QString name;
            if(nameStartIdx != -1)
            {
                QStringList nameParts;
                for(int i = nameStartIdx; i < tokens.size(); ++i)
                    nameParts << tokens[i];
                name = nameParts.join(' ');
            }
            else
            {
                name = tokens.last();
            }

            if(isLink && name.contains(" -> "))
            {
                name = name.split(" -> ").first().trimmed();
            }

            if(name == "." || name == "..")
                continue;

            AdbFileInfo info;
            info.name = name;
            info.fullPath = cleanDir + name;
            info.isDir = isDir || isLink;
            info.permissions = perms;

            if(nameStartIdx >= 3)
            {
                // Format: ... size YYYY-MM-DD HH:MM name
                bool ok = false;
                qint64 sz = tokens[nameStartIdx - 3].toLongLong(&ok);
                if(ok && sz >= 0)
                {
                    info.size = sz;
                }
                info.modifyTime = tokens[nameStartIdx - 2] + " " + tokens[nameStartIdx - 1];
            }
            else if(nameStartIdx >= 2)
            {
                info.modifyTime = tokens[nameStartIdx - 2] + " " + tokens[nameStartIdx - 1];
            }

            if(info.size == 0)
            {
                for(int i = 1; i < tokens.size() - 2; ++i)
                {
                    bool ok = false;
                    qint64 sz = tokens[i].toLongLong(&ok);
                    if(ok && sz > 0)
                    {
                        info.size = sz;
                        break;
                    }
                }
            }

            list.append(info);
        }
    }

    if(list.isEmpty())
    {
        QStringList simpleFiles = getFiles(dirPath, true);
        for(const QString &fullP : simpleFiles)
        {
            AdbFileInfo info;
            info.fullPath = fullP;
            QString base = fullP.split('/', Qt::SkipEmptyParts).last();
            info.isDir = fullP.endsWith('/');
            if(info.isDir && base.endsWith('/'))
                base.chop(1);
            info.name = base;
            list.append(info);
        }
    }

    std::sort(
        list.begin(),
        list.end(),
        [](const AdbFileInfo &a, const AdbFileInfo &b)
        {
            if(a.isDir != b.isDir)
                return a.isDir > b.isDir;
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

    return list;
}
