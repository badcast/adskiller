#include <limits>

#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QRandomGenerator>

#include "adbcmds.h"
#include "adbfront.h"

QString ADBExecFilePath()
{
    QString adbFile;
#ifdef WIN32
    adbFile = QCoreApplication::applicationDirPath() + "/adb/";
#else
    adbFile = "/usr/bin/";
#endif
    adbFile += AdbFilename;
    return adbFile;
}

std::pair<bool, QString> adb_send_cmd(int &exitCode, const QStringList &arguments)
{
    QProcess process;
    QString retval;
    process.start(ADBExecFilePath(), arguments);
    if (!process.waitForFinished())
    {
        qDebug() << "ADB Failed";
        return {false, {}};
    }
    retval = process.readAllStandardOutput();
    process.close();
    exitCode = process.exitCode();
#ifdef WIN32
    retval.replace("\r\n", "\n");
#endif
    return {true, retval};
}

std::pair<bool, QString> adb_send_cmd(const QStringList &arguments)
{
    int code;
    return adb_send_cmd(code, arguments);
}

bool operator==(const AdbDevice &lhs, const AdbDevice &rhs)
{
    return Adb::deviceHash(lhs) ==
           Adb::deviceHash(rhs);
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
    if (reply.first)
    {
        lines = reply.second.split("\n", Qt::SkipEmptyParts);
        for (const QString &line : std::as_const(lines))
        {
            if (line.startsWith("List of devices"))
                continue;
            AdbDevice dev{};
            AdbShell shell;
            dev.devId = line.split('\t').first();
            if(shell.connect(dev.devId))
            {
                dev.model = shell.getprop(CmdGetProductModel);
                dev.vendor = shell.getprop(CmdGetProductManufacturer);
                dev.displayName = dev.vendor + " " + dev.model;
            }
            devices.append(dev);
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
    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(QString{});
    bool connectStatus = shell->connect(device.devId);
    return std::make_pair(connectStatus, std::move(shell));
}

void Adb::connectFirst()
{
    QList<AdbDevice> devs = std::move(getDevices());
    if (devs.count() > 0)
        connect(devs.front().devId);
}

void Adb::connect(const QString &devId)
{
    QList<AdbDevice> devs = std::move(getDevices());
    QList<AdbDevice>::ConstIterator iter;
    iter = std::find_if(devs.cbegin(), devs.cend(), [&devId](const AdbDevice &dev)
                        { return dev.devId == devId; });
    if (iter == devs.cend())
    {
        qDebug() << "No allowed device";
        return;
    }
    if (isConnected())
        disconnect();
    device = *iter;
    emit onDeviceChanged(device, AdbConState::Add);
    deviceWatchTimer = new QTimer(this);
    deviceWatchTimer->start(100);
    QObject::connect(deviceWatchTimer, &QTimer::timeout, this, &Adb::onDeviceWatch);
}

QList<PackageIO> Adb::getPackages()
{
    QList<PackageIO> packages;
    QStringList packageList, disableList;
    QSet<QString> optimizedList;
    std::pair<bool, QString> reply;
    std::pair<bool, QString> reply2dis;
    int pkgSeprIndx = 0;
    if (!isConnected())
    {
        qDebug() << "Device is not connected";
        return {};
    }
    std::unique_ptr<AdbShell> shell = runShell().second;
    reply = shell->commandQueueWait(QStringList() << "pm" << "list" << "packages"
                                                  << "--user" << "0" << "-3");
    reply2dis = shell->commandQueueWait(QStringList() << "pm" << "list" << "packages"
                                                      << "--user" << "0" << "-3" << "-d");
    if (reply.first && reply2dis.first)
    {
        reply.second.remove('\t');
        packageList = reply.second.split('\n', Qt::SkipEmptyParts);
        disableList = reply2dis.second.split('\n', Qt::SkipEmptyParts);
        if (packageList.count() > 0)
            pkgSeprIndx = packageList.first().indexOf(':') + 1;

        std::function<QString(QString &)> removeSeperators = [pkgSeprIndx](QString &pkg)
        {
            return pkg.remove(0, pkgSeprIndx);
        };
        std::transform(std::begin(disableList), std::end(disableList), std::begin(disableList), removeSeperators);
        std::transform(std::begin(packageList), std::end(packageList), std::begin(packageList), removeSeperators);
        optimizedList = std::move(QSet<QString>(disableList.begin(), disableList.end()));

        for (QString &item : packageList)
        {
            packages.append(PackageIO{});
            packages.last().packageName = item;
            packages.last().applicationName = "Unknown";
            packages.last().disabled = optimizedList.contains(item);
        }
    }
    return packages;
}

void Adb::killPackages(const QList<PackageIO> &packages, int &successCount)
{
    std::pair<bool, QString> reply;
    successCount = 0;
    if (!isConnected())
        return;
    std::unique_ptr<AdbShell> shell = runShell().second;
    for (const PackageIO &package : packages)
    {
        reply = shell->commandQueueWait(QStringList() << "am" << "force-stop" << package.packageName);
        if (!reply.first)
            continue;
        successCount++;
    }
}

bool Adb::uninstallPackages(const QStringList &packages, int &successCount)
{
    std::pair<bool, QString> reply;
    successCount = 0;
    if (!isConnected())
        return false;
    std::unique_ptr<AdbShell> shell = runShell().second;
    for (const QString &package : packages)
    {
        reply = shell->commandQueueWait(QStringList() << "pm" << "uninstall" << "--user" << "0" << package);
        if (!reply.first)
            continue;
        successCount++;
    }
    return true;
}

bool Adb::disablePackages(const QStringList &packages, int &successCount)
{
    std::pair<bool, QString> reply;
    successCount = 0;
    if (!isConnected())
        return false;
    std::unique_ptr<AdbShell> shell = runShell().second;
    for (const QString &package : packages)
    {
        reply = shell->commandQueueWait(QStringList() << "pm" << "disable-user" << "--user" << "0" << package);
        if (!reply.first)
            continue;
        successCount++;
    }
    return true;
}

bool Adb::enablePackages(const QStringList &packages, int &successCount)
{
    std::pair<bool, QString> reply;
    successCount = 0;
    if (!isConnected())
        return false;
    std::unique_ptr<AdbShell> shell = runShell().second;
    for (const QString &package : packages)
    {
        reply = shell->commandQueueWait(QStringList() << "pm" << "enable" << "--user" << "0" << package);
        if (!reply.first)
            continue;
        successCount++;
    }
    return true;
}

void Adb::onDeviceWatch()
{
    AdbConStatus deviceStatus = status();
    if (deviceStatus != AdbConStatus::DEVICE)
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
    if (!deviceId.isEmpty())
    {
        std::pair<bool, QString> reply = adb_send_cmd(QStringList() << "-s" << deviceId << "get-state");
        if (reply.first)
        {
            res = reply.second.remove("\n");
            if (res == "unathourized")
                retval = UNAUTH;
            else if (res == "device")
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
    if (!isConnected())
    {
        qDebug() << "No connected";
        return;
    }
    AdbDevice old = device;
    device = {};
    emit onDeviceChanged(old, AdbConState::Removed);
    if (deviceWatchTimer)
    {
        deviceWatchTimer->stop();
        delete deviceWatchTimer;
        deviceWatchTimer = nullptr;
    }
}
