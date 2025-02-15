#ifndef ADBTRACE_H
#define ADBTRACE_H

#include <QList>
#include <QProcess>
#ifdef _WIN32
#include <windows.h>
#endif

#include "packages.h"

struct AdbDevice
{
    QString devId;
    QString model;
    QString displayName;
    QString vendor;
};

class Adb
{
private:
    void fetch();
public:
    AdbDevice device;
    Adb();
    bool isConnected();
    void connectFirst();
    void connect(const QString& devId);
    void disconnect();
    QList<PackageIO> getPackages();

    static QList<AdbDevice> devices();
};

#endif // ADBTRACE_H
