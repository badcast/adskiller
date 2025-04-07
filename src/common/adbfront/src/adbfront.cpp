#include <utility>

#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

#include "adbfront.h"
#include "adbcmds.h"

QString ADBExecFilename()
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

std::pair<bool,QString> adb_send_cmd(int& exitCode, const QStringList & arguments)
{
    QProcess process;
    QString retval;
    process.start(ADBExecFilename(), arguments);
    if(!process.waitForFinished())
    {
        qDebug() << "ADB Failed";
        return {false,{}};
    }
    retval = process.readAllStandardOutput();
    process.close();
    exitCode = process.exitCode();
#ifdef WIN32
    retval.replace("\r\n", "\n");
#endif
    return {true,retval};
}

std::pair<bool,QString> adb_send_cmd(const QStringList & arguments)
{
    int code;
    return adb_send_cmd(code, arguments);
}

QString adb_get_prop(const QString & devId, const QString& prop)
{
    QString output = adb_send_cmd(QStringList() << "-s" << devId << "shell" << "getprop" << prop).second.remove("\n");
    return output;
}

bool operator ==(const AdbDevice& lhs, const AdbDevice& rhs)
{
    return Adb::deviceHash(lhs) ==
           Adb::deviceHash(rhs);
}

Adb::Adb(QObject * parent) : QObject(parent), deviceWatchTimer(nullptr)
{

}

Adb::~Adb()
{
    adb_send_cmd(QStringList() << "kill-server");
}

AdbConStatus Adb::status() const
{
    return deviceStatus(device);
}

QList<AdbDevice> Adb::getDevices()
{
    QList<AdbDevice> devices;
    QStringList lines;
    std::pair<bool,QString> reply = adb_send_cmd(QStringList() << "devices");
    if(reply.first)
    {
        lines = reply.second.split("\n", Qt::SkipEmptyParts);
        for(const QString & line : std::as_const(lines))
        {
            if(line.startsWith("List of devices"))
                continue;
            AdbDevice dev{};
            dev.devId = line.split('\t').first();
            dev.model = adb_get_prop(dev.devId,CmdGetProductModel);
            dev.vendor = adb_get_prop(dev.devId,CmdGetProductManufacturer);
            dev.displayName = dev.vendor + " " + dev.model;
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

bool Adb::isConnected()
{
    return status() == DEVICE;
}

void Adb::connectFirst()
{
    QList<AdbDevice> devs = getDevices();
    if(devs.count() > 0)
        connect(devs.front().devId);
}

void Adb::connect(const QString &devId)
{
    QList<AdbDevice> devs = getDevices();
    QList<AdbDevice>::ConstIterator iter;
    iter = std::find_if(devs.cbegin(), devs.cend(), [&devId](const AdbDevice & dev) { return dev.devId == devId; });
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

QList<PackageIO> Adb::getPackages()
{
    QList<PackageIO> packages;
    QStringList packageList;

    if(!isConnected())
    {
        qDebug() << "Device is not connected";
        return {};
    }

    std::pair<bool,QString> reply = adb_send_cmd(QStringList() << "-s" << device.devId << "shell" << "pm" << "list" << "packages" << "-3");
    if(reply.first)
    {
        reply.second.remove('\t');
        packageList = reply.second.split('\n', Qt::SkipEmptyParts);
        for(QString & item : packageList)
        {
            item.remove(0, item.indexOf(':')+1);
            packages.append(PackageIO{});
            packages.last().packageName = item;
            packages.last().applicationName = "Unknown";
        }
    }
    return packages;
}

void Adb::killPackages(const QList<PackageIO> &packages)
{
    std::pair<bool,QString> reply;
    int successCount = 0;
    int exit;
    if(!isConnected())
        return;
    for(const PackageIO & package : packages)
    {
        reply = adb_send_cmd(exit, QStringList() << "shell" << "am" << "force-stop" << package.packageName);
        if(!reply.first || exit != 0)
            return;
        successCount++;
        // QString out = process.readAllStandardOutput();
        // if(out == "Success")
        // {
        // }
    }
}

bool Adb::uninstallPackages(const QStringList &packages, int& successCount)
{
    std::pair<bool,QString> reply;
    int exit;
    if(!isConnected())
        return false;
    for(const QString & package : packages)
    {
        reply = adb_send_cmd(exit, QStringList() << "uninstall" << package);
        if(!reply.first || exit != 0)
            return false;
        successCount++;
        // QString out = process.readAllStandardOutput();
        // if(out == "Success")
        // {
        // }
    }
    return true;
}

void Adb::onDeviceWatch()
{
    AdbConStatus s = status();
    if(s != AdbConStatus::DEVICE)
    {
        AdbDevice old = device;
        devices.removeOne(device);
        device = {};
        emit onDeviceChanged(old, AdbConState::Removed);
        deviceWatchTimer->stop();
        deviceWatchTimer->deleteLater();
        deviceWatchTimer = nullptr;
    }
}

AdbConStatus Adb::deviceStatus(const AdbDevice &device)
{
    AdbConStatus retval = UNKNOWN;
    QString res,tmp = device.devId;
    if(!tmp.isEmpty())
    {
        std::pair<bool,QString> reply = adb_send_cmd(QStringList() << "-s" << tmp << "get-state");
        if(reply.first)
        {
            res = reply.second.remove("\n");
            if(res == "unathourized")
                retval = UNAUTH;
            else if(res == "device")
                retval = DEVICE;
        }
        else
        {
            qDebug() << "Failed retry get-state";
        }
    }
    return retval;
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
