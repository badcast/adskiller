#include "applefront.h"

#include <algorithm>
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>

bool AppleDevice::isEmpty() const
{
    return devId.isEmpty() && model.isEmpty() && displayName.isEmpty();
}

QString AppleDevice::modeString() const
{
    switch(mode)
    {
        case AppleDeviceMode::Normal:
            return QString::fromUtf8("Normal (Включен)");
        case AppleDeviceMode::Recovery:
            return QString::fromUtf8("Recovery Mode");
        case AppleDeviceMode::DFU:
            return QString::fromUtf8("DFU Mode");
        default:
            return QString::fromUtf8("Не подключено");
    }
}

QString AppleSysInfo::OSVersionString() const
{
    return QString("iOS %1 (%2)").arg(productVersion, buildVersion);
}

QString AppleSysInfo::StorageDesignString() const
{
    if(diskTotal <= 0)
        return QString("—");
    double usedGB = diskUsed / (1024.0 * 1024.0 * 1024.0);
    double totalGB = diskTotal / (1024.0 * 1024.0 * 1024.0);
    return QString("%1 ГБ / %2 ГБ").arg(usedGB, 0, 'f', 1).arg(totalGB, 0, 'f', 1);
}

QString AppleSysInfo::BatteryDesignString() const
{
    if(batteryLevel < 0)
        return QString("—");
    return QString("%1% %2").arg(batteryLevel).arg(isCharging ? QString::fromUtf8("(Зарядка)") : QString());
}

bool operator==(const AppleDevice &lhs, const AppleDevice &rhs)
{
    return Apple::deviceHash(lhs) == Apple::deviceHash(rhs);
}

QString AppleExecutableFilename(const QString &program)
{
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef WIN32
    QStringList candidates = {appDir + "/apple/" + program + ".exe", appDir + "/bin/apple/" + program + ".exe", appDir + "/" + program + ".exe", program + ".exe"};
    for(const QString &path : candidates)
    {
        if(QFile::exists(path))
            return path;
    }
    return program + ".exe";
#else
    QStringList candidates = {appDir + "/apple/" + program, appDir + "/bin/apple/" + program, "/usr/bin/" + program, program};
    for(const QString &path : candidates)
    {
        if(QFile::exists(path))
            return path;
    }
    return program;
#endif
}

std::pair<bool, QString> apple_send_cmd(int &exitCode, const QString &program, const QStringList &arguments)
{
    QProcess process;
    QString exe = AppleExecutableFilename(program);
    process.start(exe, arguments);
    if(!process.waitForFinished(5000))
    {
        process.kill();
        process.waitForFinished(1000);
        exitCode = -1;
        return {false, {}};
    }
    exitCode = process.exitCode();
    QString retval = QString::fromUtf8(process.readAllStandardOutput());
    QString errval = QString::fromUtf8(process.readAllStandardError());
    if(exitCode != 0 && retval.isEmpty())
        retval = errval;
    return {exitCode == 0, retval};
}

std::pair<bool, QString> apple_send_cmd(const QString &program, const QStringList &arguments)
{
    int code = 0;
    return apple_send_cmd(code, program, arguments);
}

QString Apple::marketingNameForModel(const QString &model)
{
    static const QHash<QString, QString> s_models = {
        // iPhone
        {"iPhone7,1", "iPhone 6 Plus"},
        {"iPhone7,2", "iPhone 6"},
        {"iPhone8,1", "iPhone 6s"},
        {"iPhone8,2", "iPhone 6s Plus"},
        {"iPhone8,4", "iPhone SE (1st gen)"},
        {"iPhone9,1", "iPhone 7"},
        {"iPhone9,2", "iPhone 7 Plus"},
        {"iPhone9,3", "iPhone 7"},
        {"iPhone9,4", "iPhone 7 Plus"},
        {"iPhone10,1", "iPhone 8"},
        {"iPhone10,2", "iPhone 8 Plus"},
        {"iPhone10,3", "iPhone X"},
        {"iPhone10,4", "iPhone 8"},
        {"iPhone10,5", "iPhone 8 Plus"},
        {"iPhone10,6", "iPhone X"},
        {"iPhone11,2", "iPhone XS"},
        {"iPhone11,4", "iPhone XS Max"},
        {"iPhone11,6", "iPhone XS Max"},
        {"iPhone11,8", "iPhone XR"},
        {"iPhone12,1", "iPhone 11"},
        {"iPhone12,3", "iPhone 11 Pro"},
        {"iPhone12,5", "iPhone 11 Pro Max"},
        {"iPhone12,8", "iPhone SE (2nd gen)"},
        {"iPhone13,1", "iPhone 12 mini"},
        {"iPhone13,2", "iPhone 12"},
        {"iPhone13,3", "iPhone 12 Pro"},
        {"iPhone13,4", "iPhone 12 Pro Max"},
        {"iPhone14,2", "iPhone 13 Pro"},
        {"iPhone14,3", "iPhone 13 Pro Max"},
        {"iPhone14,4", "iPhone 13 mini"},
        {"iPhone14,5", "iPhone 13"},
        {"iPhone14,6", "iPhone SE (3rd gen)"},
        {"iPhone14,7", "iPhone 14"},
        {"iPhone14,8", "iPhone 14 Plus"},
        {"iPhone15,2", "iPhone 14 Pro"},
        {"iPhone15,3", "iPhone 14 Pro Max"},
        {"iPhone15,4", "iPhone 15"},
        {"iPhone15,5", "iPhone 15 Plus"},
        {"iPhone16,1", "iPhone 15 Pro"},
        {"iPhone16,2", "iPhone 15 Pro Max"},
        {"iPhone17,1", "iPhone 16 Pro"},
        {"iPhone17,2", "iPhone 16 Pro Max"},
        {"iPhone17,3", "iPhone 16"},
        {"iPhone17,4", "iPhone 16 Plus"},
        // iPad
        {"iPad6,11", "iPad (5th gen)"},
        {"iPad6,12", "iPad (5th gen)"},
        {"iPad7,5", "iPad (6th gen)"},
        {"iPad7,6", "iPad (6th gen)"},
        {"iPad7,11", "iPad (7th gen)"},
        {"iPad7,12", "iPad (7th gen)"},
        {"iPad11,6", "iPad (8th gen)"},
        {"iPad11,7", "iPad (8th gen)"},
        {"iPad12,1", "iPad (9th gen)"},
        {"iPad12,2", "iPad (9th gen)"},
        {"iPad13,18", "iPad (10th gen)"},
        {"iPad13,19", "iPad (10th gen)"},
        {"iPad11,3", "iPad Air (3rd gen)"},
        {"iPad11,4", "iPad Air (3rd gen)"},
        {"iPad13,1", "iPad Air (4th gen)"},
        {"iPad13,2", "iPad Air (4th gen)"},
        {"iPad13,16", "iPad Air (5th gen)"},
        {"iPad13,17", "iPad Air (5th gen)"},
        {"iPad11,1", "iPad mini (5th gen)"},
        {"iPad11,2", "iPad mini (5th gen)"},
        {"iPad14,1", "iPad mini (6th gen)"},
        {"iPad14,2", "iPad mini (6th gen)"},
        {"iPad8,1", "iPad Pro 11-inch (1st gen)"},
        {"iPad8,2", "iPad Pro 11-inch (1st gen)"},
        {"iPad8,3", "iPad Pro 11-inch (1st gen)"},
        {"iPad8,4", "iPad Pro 11-inch (1st gen)"},
        {"iPad8,9", "iPad Pro 11-inch (2nd gen)"},
        {"iPad8,10", "iPad Pro 11-inch (2nd gen)"},
        {"iPad13,4", "iPad Pro 11-inch (3rd gen)"},
        {"iPad13,5", "iPad Pro 11-inch (3rd gen)"},
        {"iPad13,6", "iPad Pro 11-inch (3rd gen)"},
        {"iPad13,7", "iPad Pro 11-inch (3rd gen)"},
        {"iPad14,3", "iPad Pro 11-inch (4th gen)"},
        {"iPad14,4", "iPad Pro 11-inch (4th gen)"},
        {"iPad8,5", "iPad Pro 12.9-inch (3rd gen)"},
        {"iPad8,6", "iPad Pro 12.9-inch (3rd gen)"},
        {"iPad8,7", "iPad Pro 12.9-inch (3rd gen)"},
        {"iPad8,8", "iPad Pro 12.9-inch (3rd gen)"},
        {"iPad8,11", "iPad Pro 12.9-inch (4th gen)"},
        {"iPad8,12", "iPad Pro 12.9-inch (4th gen)"},
        {"iPad13,8", "iPad Pro 12.9-inch (5th gen)"},
        {"iPad13,9", "iPad Pro 12.9-inch (5th gen)"},
        {"iPad13,10", "iPad Pro 12.9-inch (5th gen)"},
        {"iPad13,11", "iPad Pro 12.9-inch (5th gen)"},
        {"iPad14,5", "iPad Pro 12.9-inch (6th gen)"},
        {"iPad14,6", "iPad Pro 12.9-inch (6th gen)"}};
    return s_models.value(model, model);
}

uint Apple::deviceHash(const AppleDevice &device)
{
    uint retval;
    QString _compare;
    _compare += device.devId;
    _compare += device.model;
    _compare += device.vendor;
    _compare += QString::number(static_cast<int>(device.mode));
    retval = qHash(_compare);
    return retval;
}

AppleDevice Apple::getDevice(const QString &deviceId)
{
    AppleDevice dev;
    dev.devId = deviceId;
    dev.vendor = "Apple";

    if(deviceId.startsWith("Apple-Recovery"))
    {
        dev.mode = AppleDeviceMode::Recovery;
        dev.isPaired = true;
        dev.model = "Apple Device";
        dev.marketingName = "Apple Device (Recovery)";
        dev.displayName = "Apple Device (Recovery Mode)";
        return dev;
    }
    if(deviceId.startsWith("Apple-DFU"))
    {
        dev.mode = AppleDeviceMode::DFU;
        dev.isPaired = true;
        dev.model = "Apple Device";
        dev.marketingName = "Apple Device (DFU)";
        dev.displayName = "Apple Device (DFU Mode)";
        return dev;
    }

    // Validate pairing
    int exitCode = 0;
    auto [pairOk, pairOut] = apple_send_cmd(exitCode, "idevicepair", QStringList() << "-u" << deviceId << "validate");
    if(pairOk && pairOut.contains("SUCCESS"))
    {
        dev.isPaired = true;
        dev.mode = AppleDeviceMode::Normal;

        AppleShell shell(deviceId);
        dev.model = shell.getprop("ProductType");
        dev.productVersion = shell.getprop("ProductVersion");
        dev.buildVersion = shell.getprop("BuildVersion");
        dev.serialNumber = shell.getprop("SerialNumber");
        dev.ecid = shell.getprop("UniqueChipID");
        QString devName = shell.getprop("DeviceName");

        dev.marketingName = Apple::marketingNameForModel(dev.model);
        dev.displayName = !devName.isEmpty() ? devName : (!dev.marketingName.isEmpty() ? dev.marketingName : dev.model);
    }
    else
    {
        dev.isPaired = false;
        dev.mode = AppleDeviceMode::Normal;
        dev.model = "iPhone/iPad";
        dev.marketingName = "Apple Device";
        dev.displayName = QString::fromUtf8("Apple Device (Не доверенное)");
        // Trigger pairing prompt on iOS screen
        apple_send_cmd(exitCode, "idevicepair", QStringList() << "-u" << deviceId << "pair");
    }
    return dev;
}

QList<AppleDevice> Apple::getDevices()
{
    QList<AppleDevice> devices;

    // 1. Normal mode devices via idevice_id
    int exitCode = 0;
    auto [ok, out] = apple_send_cmd(exitCode, "idevice_id", QStringList() << "-l");
    if(ok && !out.isEmpty())
    {
        QStringList lines = out.split('\n', Qt::SkipEmptyParts);
        for(QString &line : lines)
        {
            QString udid = line.trimmed();
            if(!udid.isEmpty())
            {
                devices.append(getDevice(udid));
            }
        }
    }

    // 2. Recovery / DFU mode devices via irecovery
    auto [recOk, recOut] = apple_send_cmd(exitCode, "irecovery", QStringList() << "-q");
    if(recOk && !recOut.isEmpty() && recOut.contains("MODE:"))
    {
        AppleDevice recDev;
        QStringList recLines = recOut.split('\n', Qt::SkipEmptyParts);
        for(const QString &rLine : recLines)
        {
            int colonIdx = rLine.indexOf(':');
            if(colonIdx != -1)
            {
                QString k = rLine.left(colonIdx).trimmed();
                QString v = rLine.mid(colonIdx + 1).trimmed();
                if(k == "PRODUCT")
                    recDev.marketingName = v;
                else if(k == "MODEL")
                    recDev.model = v;
                else if(k == "ECID")
                    recDev.ecid = v;
                else if(k == "MODE")
                {
                    if(v.contains("DFU", Qt::CaseInsensitive))
                        recDev.mode = AppleDeviceMode::DFU;
                    else
                        recDev.mode = AppleDeviceMode::Recovery;
                }
            }
        }
        recDev.devId = !recDev.ecid.isEmpty() ? recDev.ecid : QString("Apple-Recovery");
        recDev.displayName = recDev.marketingName + (recDev.mode == AppleDeviceMode::DFU ? QString(" (DFU Mode)") : QString(" (Recovery Mode)"));
        recDev.isPaired = true;
        devices.append(recDev);
    }

    // 3. Fallback via USB device query if not detected
    if(devices.isEmpty())
    {
#ifdef WIN32
        auto [pnpOk, pnpOut] = apple_send_cmd(exitCode, "powershell", QStringList() << "-NoProfile" << "-Command" << "Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like '*VID_05AC*' } | Select-Object -ExpandProperty InstanceId");
        if(pnpOk && pnpOut.contains("VID_05AC", Qt::CaseInsensitive))
        {
            if(pnpOut.contains("PID_1281", Qt::CaseInsensitive))
            {
                AppleDevice d;
                d.devId = "Apple-Recovery";
                d.model = "Apple Device";
                d.marketingName = "Apple Device (Recovery)";
                d.displayName = QString::fromUtf8("Apple Device (Recovery Mode)");
                d.mode = AppleDeviceMode::Recovery;
                d.isPaired = true;
                devices.append(d);
            }
            else if(pnpOut.contains("PID_1227", Qt::CaseInsensitive))
            {
                AppleDevice d;
                d.devId = "Apple-DFU";
                d.model = "Apple Device";
                d.marketingName = "Apple Device (DFU)";
                d.displayName = QString::fromUtf8("Apple Device (DFU Mode)");
                d.mode = AppleDeviceMode::DFU;
                d.isPaired = true;
                devices.append(d);
            }
        }
#else
        auto [usbOk, usbOut] = apple_send_cmd(exitCode, "lsusb", QStringList());
        if(usbOk && usbOut.contains("05ac:"))
        {
            QStringList usbLines = usbOut.split('\n', Qt::SkipEmptyParts);
            for(const QString &uLine : usbLines)
            {
                if(uLine.contains("05ac:1281"))
                {
                    AppleDevice d;
                    d.devId = "Apple-Recovery";
                    d.model = "Apple Device";
                    d.marketingName = "Apple Device (Recovery)";
                    d.displayName = QString::fromUtf8("Apple Device (Recovery Mode)");
                    d.mode = AppleDeviceMode::Recovery;
                    d.isPaired = true;
                    devices.append(d);
                    break;
                }
                else if(uLine.contains("05ac:1227"))
                {
                    AppleDevice d;
                    d.devId = "Apple-DFU";
                    d.model = "Apple Device";
                    d.marketingName = "Apple Device (DFU)";
                    d.displayName = QString::fromUtf8("Apple Device (DFU Mode)");
                    d.mode = AppleDeviceMode::DFU;
                    d.isPaired = true;
                    devices.append(d);
                    break;
                }
            }
        }
#endif
    }

    return devices;
}

AppleConStatus Apple::deviceStatus(const QString &deviceId)
{
    if(deviceId.isEmpty())
        return APPLE_UNKNOWN;

    if(deviceId.startsWith("Apple-Recovery"))
        return APPLE_RECOVERY;
    if(deviceId.startsWith("Apple-DFU"))
        return APPLE_DFU;

    int exitCode = 0;
    auto [pairOk, pairOut] = apple_send_cmd(exitCode, "idevicepair", QStringList() << "-u" << deviceId << "validate");
    if(pairOk && pairOut.contains("SUCCESS"))
    {
        return APPLE_DEVICE;
    }

    // Check if listed in idevice_id
    auto [idOk, idOut] = apple_send_cmd(exitCode, "idevice_id", QStringList() << "-l");
    if(idOk && idOut.contains(deviceId))
    {
        return APPLE_UNAUTH;
    }

    // Check recovery
    auto [recOk, recOut] = apple_send_cmd(exitCode, "irecovery", QStringList() << "-q");
    if(recOk && recOut.contains("MODE:"))
    {
        if(recOut.contains("DFU", Qt::CaseInsensitive))
            return APPLE_DFU;
        return APPLE_RECOVERY;
    }

    return APPLE_UNKNOWN;
}

AppleConStatus Apple::deviceStatus(const AppleDevice &device)
{
    return Apple::deviceStatus(device.devId);
}

Apple::Apple(QObject *parent) : QObject(parent), deviceWatchTimer(nullptr)
{
}

Apple::~Apple()
{
    disconnect();
}

AppleConStatus Apple::status() const
{
    return deviceStatus(device.devId);
}

bool Apple::isConnected()
{
    AppleConStatus s = status();
    return s == APPLE_DEVICE || s == APPLE_RECOVERY || s == APPLE_DFU;
}

void Apple::connectFirst()
{
    QList<AppleDevice> devs = getDevices();
    if(!devs.isEmpty())
        connect(devs.front().devId);
}

void Apple::connect(const QString &devId)
{
    QList<AppleDevice> devs = getDevices();
    auto iter = std::find_if(devs.cbegin(), devs.cend(), [&devId](const AppleDevice &d) { return d.devId == devId; });
    if(iter == devs.cend())
    {
        return;
    }
    if(isConnected())
        disconnect();
    device = *iter;
    emit onDeviceChanged(device, AppleConState::AppleAdd);
    deviceWatchTimer = new QTimer(this);
    deviceWatchTimer->start(200);
    QObject::connect(deviceWatchTimer, &QTimer::timeout, this, &Apple::onDeviceWatch);
}

void Apple::disconnect()
{
    if(device.isEmpty())
        return;
    AppleDevice old = device;
    device = {};
    emit onDeviceChanged(old, AppleConState::AppleRemoved);
    if(deviceWatchTimer)
    {
        deviceWatchTimer->stop();
        delete deviceWatchTimer;
        deviceWatchTimer = nullptr;
    }
}

std::pair<bool, std::unique_ptr<AppleShell>> Apple::runShell()
{
    auto shell = std::make_unique<AppleShell>(device.devId);
    bool ok = shell->connect(device.devId);
    return {ok, std::move(shell)};
}

void Apple::onDeviceWatch()
{
    AppleConStatus cur = status();
    if(cur == APPLE_UNKNOWN)
    {
        disconnect();
    }
}

#include "moc_applefront.cpp"
