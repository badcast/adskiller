#include <QStringList>
#include <QDebug>

#include "adbtrace.h"
#include "adbcmds.h"

QString adb_get_prop(const QString & devId, const QString& prop)
{
    QProcess process;
    process.start(QString(cmd_adb), QStringList() << "-s" << devId << "shell" << "getprop" << prop);
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

Adb::Adb(QObject * parent) : QObject(parent), deviceWatchTimer(nullptr)
{

}

AdbConStatus Adb::status() const
{
    return deviceStatus(device);
}

QList<AdbDevice> Adb::devices()
{
    QList<AdbDevice> devices;
    QStringList lines;
    QProcess process;

    process.start(QString(cmd_adb), QStringList() << "kill-server");
    process.waitForFinished();
    process.start(QString(cmd_adb), QStringList() << "start-server");
    process.waitForFinished();
    process.start(QString(cmd_adb), QStringList() << "devices");
    if(!process.waitForFinished())
    {
        qDebug() << "Error on run command";
        return {};
    }
    QString output = process.readAllStandardOutput();
    process.close();
    lines = output.split('\n', Qt::SkipEmptyParts);
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

bool Adb::isConnected()
{
    return status() == DEVICE;
}

void Adb::connectFirst()
{
    QList<AdbDevice> devs = devices();
    if(devs.count() > 0)
        connect(devs.front().devId);
}

void Adb::connect(const QString &devId)
{
    QList<AdbDevice> devs = devices();
    QList<AdbDevice>::ConstIterator iter;
    iter = std::find_if(devs.cbegin(), devs.cend(), [&devId](const AdbDevice & dev) { return dev.devId == devId; });
    if(iter == devs.cend())
    {
        qDebug() << "No allowed device";
        return;
    }
    if(isConnected())
        emit onDeviceChanged(device, AdbConState::Removed);
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
    process.start(QString(cmd_adb), QStringList() << "-s" << device.devId << "shell" << "pm" << "list" << "packages" << "-3");
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
    packageList=output.split('\n', Qt::SkipEmptyParts);
    for(QString & item : packageList)
    {
        item.remove(0, item.indexOf(':')+1);
        packages.append(PackageIO{});
        packages.last().packageName = item;
        packages.last().applicationName = "Unknown";
    }
    return packages;
}

void Adb::onDeviceWatch()
{
    AdbConStatus s = status();
    if(s != AdbConStatus::DEVICE)
    {
        emit onDeviceChanged(device, AdbConState::Removed);
        deviceWatchTimer->deleteLater();
        device = {};
    }
}

AdbConStatus Adb::deviceStatus(const AdbDevice &device)
{
    AdbConStatus retval = UNKNOWN;
    QString res,tmp = device.devId;
    if(!tmp.isEmpty())
    {
        QProcess proc;
        proc.start(cmd_adb, QStringList() << "-s" << tmp << "get-state");
        if(proc.waitForFinished())
        {
            res = proc.readAllStandardOutput();
            res.remove('\n');
            if(res == "unthourized")
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
    emit onDeviceChanged(device, AdbConState::Removed);
    if(deviceWatchTimer)
    {
        delete deviceWatchTimer;
        deviceWatchTimer = nullptr;
    }
    device = {};
}
