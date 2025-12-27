#include <QTextStream>

#include "adbcmds.h"
#include "adbfront.h"

std::shared_ptr<AdbSysInfo> AdbShell::getInfo() {
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
        if(at.startsWith("Filesystem",Qt::CaseInsensitive))
            continue;
        tmp0 = std::move(at.split(' ', Qt::SkipEmptyParts));
        sysi->diskTotal = tmp0[1].toULongLong();
        sysi->diskUsed = tmp0[2].toULongLong();
        break;
    }

    txt.setString(&freeDataChars);
    while(!txt.atEnd())
    {
        QString at = txt.readLine();

        tmp0 = std::move(at.split(' ', Qt::SkipEmptyParts));
        if(at.startsWith("Mem",Qt::CaseInsensitive))
        {
            sysi->ramTotal = tmp0[1].toULongLong();
            sysi->ramUsed = tmp0[2].toULongLong();
        }
        else if(at.startsWith("swap", Qt::CaseInsensitive))
        {
            sysi->swapTotal = tmp0[1].toULongLong();
            sysi->swapUsed = tmp0[2].toULongLong();
            sysi->swapEnabled = sysi->swapTotal > 0;
        }
    }

    sysi->osVersion = getprop(PropAndroidVersion).toInt();
    sysi->systemName = commandQueueWaits("uname -s").second;
    sysi->machine = commandQueueWaits("uname -m").second;
    sysi->kernelReleaseVersion = commandQueueWaits("uname -r").second;
    sysi->kernelVersion = commandQueueWaits("uname -v").second;
    sysi->isAndroid = commandQueueWaits("getprop | grep -i android").second.length() > 0;

    if(!isConnect())
        return nullptr;

    return sysi;
}

QString AdbSysInfo::OSVersionString() const
{
    return QString("%1 %2").arg((isAndroid ? "Android" : "Unknown")).arg( (osVersion == 0) ? NA : QString("%1").arg(osVersion));
}
