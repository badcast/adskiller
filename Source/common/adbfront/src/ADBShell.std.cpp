#include <array>

#include <QTextStream>

#include "Extension.h"
#include "AdbCmds.h"
#include "AdbFront.h"

std::shared_ptr<AdbSysInfo> AdbShell::getInfo()
{
    if(!isConnect())
        return nullptr;
    std::shared_ptr<AdbSysInfo> sysi = std::make_shared<AdbSysInfo>();
    QString dfDataChars = std::move(commandQueueWaits("df /data").second);
    QString freeDataChars = std::move(commandQueueWaits("free").second);
    QStringList tmp0;
    QTextStream txt(&dfDataChars);
    while(!txt.atEnd())
    {
        QString at = txt.readLine();
        if(at.startsWith("Filesystem", Qt::CaseInsensitive))
            continue;
        tmp0 = std::move(at.split(' ', Qt::SkipEmptyParts));
        if(tmp0.size() >= 3)
        {
            sysi->diskTotal = tmp0[1].toULongLong() * 1024;
            sysi->diskUsed = tmp0[2].toULongLong() * 1024;
            break;
        }
    }

    txt.setString(&freeDataChars);
    while(!txt.atEnd())
    {
        QString at = txt.readLine();

        tmp0 = std::move(at.split(' ', Qt::SkipEmptyParts));
        if(at.startsWith("mem", Qt::CaseInsensitive) && tmp0.size() >= 3)
        {
            sysi->ramTotal = tmp0[1].toULongLong();
            sysi->ramUsed = tmp0[2].toULongLong();
        }
        else if(at.startsWith("swap", Qt::CaseInsensitive) && tmp0.size() >= 3)
        {
            sysi->swapTotal = tmp0[1].toULongLong();
            sysi->swapUsed = tmp0[2].toULongLong();
            sysi->swapEnabled = sysi->swapTotal > 0;
        }
    }

    sysi->osVersion = getprop(PropAndroidVersion).toInt();
    sysi->systemName = commandQueueWaits("uname -s").second.trimmed();
    sysi->machine = commandQueueWaits("uname -m").second.trimmed();
    sysi->kernelReleaseVersion = commandQueueWaits("uname -r").second.trimmed();
    sysi->kernelVersion = commandQueueWaits("uname -v").second.trimmed();
    sysi->isAndroid = commandQueueWaits("getprop | grep -i android").second.length() > 0;

    if(!isConnect())
        return nullptr;

    return sysi;
}

QString AdbSysInfo::OSVersionString() const
{
    return QString("%1 %2").arg((isAndroid ? "Android" : "Unknown"), ((osVersion == 0) ? NA : QString::number(osVersion)));
}

QString AdbSysInfo::StorageDesignString() const
{
    auto ret = Generic::formatDesignSize(diskTotal);
    return QString("%1%2").arg(ret.first).arg(ret.second);
}

QString AdbSysInfo::RAMDesignString() const
{
    auto ret = Generic::formatDesignSize(ramTotal);
    return QString("%1%2").arg(ret.first).arg(ret.second);
}
