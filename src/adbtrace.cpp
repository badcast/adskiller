#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

#include "adbtrace.h"
#include "adbcmds.h"

QString adbExec()
{
    QString adbFile;
#ifdef WIN32
    adbFile = QCoreApplication::applicationDirPath() + "/adb/";
#else
    adbFile = "/usr/bin/";
#endif
    adbFile += cmd_adb;
    return adbFile;
}

QString adb_get_prop(const QString & devId, const QString& prop)
{
    QProcess process;
    process.start(adbExec(), QStringList() << "-s" << devId << "shell" << "getprop" << prop);
    if(!process.waitForFinished())
    {
        qDebug() << "Prop timedout";
        return {};
    }
    QString output = process.readAllStandardOutput();
    process.close();
    output.remove('\n');
    output.remove('\r');
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

AdbConStatus Adb::status() const
{
    return deviceStatus(device);
}

QList<AdbDevice> Adb::getDevices()
{
    QList<AdbDevice> devices;
    QStringList lines;
    QProcess process;
    process.start(adbExec(), QStringList() << "kill-server");
    process.waitForFinished();
    process.start(adbExec(), QStringList() << "start-server");
    process.waitForFinished();
    process.start(adbExec(), QStringList() << "devices");
    if(!process.waitForFinished())
    {
        qDebug() << "Error on run command";
        return {};
    }
    QString output = process.readAllStandardOutput();
    process.close();
    lines = output.replace("\r\n", "\n").split("\n", Qt::SkipEmptyParts);
    for(const QString & line : std::as_const(lines))
    {
        if(line.startsWith("List of devices"))
            continue;
        AdbDevice dev{};
        dev.devId = line.split('\t').first();
        dev.model = adb_get_prop(dev.devId,CmdGetProductModel);
        dev.displayName = adb_get_prop(dev.devId,CmdGetProductMarketname);
        dev.vendor = adb_get_prop(dev.devId,CmdGetProductManufacturer);
        dev.displayName = dev.vendor + " " + dev.model;
        devices.append(dev);
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
    QProcess process;
    if(!isConnected())
    {
        qDebug() << "Device is not connected";
        return {};
    }
    process.start(adbExec(), QStringList() << "-s" << device.devId << "shell" << "pm" << "list" << "packages" << "-3");
    if(!process.waitForFinished())
    {
        qDebug() << "Prop timedout";
        return {};
    }
    QStringList packageList;
    QList<PackageIO> packages;
    QString output = process.readAllStandardOutput();
    process.close();
    output.remove('\r');
    output.remove('\t');
    packageList=output.replace("\r\n", "\n").split('\n', Qt::SkipEmptyParts);
    for(QString & item : packageList)
    {
        item.remove(0, item.indexOf(':')+1);
        packages.append(PackageIO{});
        packages.last().packageName = item;
        packages.last().applicationName = "Unknown";
    }
    return packages;
}

bool Adb::uninstallPackages(const QStringList &packages)
{
    QProcess process;
    if(!isConnected())
        return false;

    for(const QString & package : packages)
    {
        process.start(adbExec(), QStringList() << "uninstall" << package);
        if(!process.waitForFinished() || process.exitCode() != 0)
            return false;
        // QString out = process.readAllStandardOutput();
        // if(out == "Success")
        // {
        // }
        process.close();
    }
    return true;
}

void Adb::onDeviceWatch()
{
    AdbConStatus s = status();
    if(s != AdbConStatus::DEVICE)
    {
        deviceWatchTimer->stop();
        AdbDevice old = device;
        devices.removeOne(device);
        device = {};
        emit onDeviceChanged(old, AdbConState::Removed);
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
        QProcess proc;
        proc.start(adbExec(), QStringList() << "-s" << tmp << "get-state");
        if(proc.waitForFinished())
        {
            res = proc.readAllStandardOutput();
            res.remove('\n').remove('\r');
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
