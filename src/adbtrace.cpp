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

Adb::Adb()
{
}

QList<AdbDevice> Adb::devices()
{
    QList<AdbDevice> devices;
    QStringList lines;
    QProcess process;
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
    return !device.devId.isEmpty();
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
    device = *iter;
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

void Adb::disconnect()
{
    if(!isConnected())
    {
        qDebug() << "No connected";
        return;
    }
    device = {};
}
