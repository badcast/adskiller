#ifndef ADBTRACE_H
#define ADBTRACE_H

#ifdef _WIN32
#include <windows.h>
#endif

#include <QList>
#include <QProcess>
#include <QTimer>


#include "packages.h"

enum AdbConStatus
{
    UNKNOWN,
    UNAUTH,
    DEVICE
};

enum AdbConState
{
    Removed,
    Add
};

struct AdbDevice
{
    QString devId;
    QString model;
    QString displayName;
    QString vendor;
};


class Adb : public QObject
{
    Q_OBJECT

private:
    void fetch();

public:
    AdbDevice device;
    Adb(QObject * parent = nullptr);
    AdbConStatus status() const;
    bool isConnected();
    void connectFirst();
    void connect(const QString& devId);
    void disconnect();
    QList<PackageIO> getPackages();

signals:
    void onDeviceChanged(const AdbDevice& device, AdbConState state);
private slots:
    void onDeviceWatch();
public:
    static AdbConStatus deviceStatus(const AdbDevice &device);
    static QList<AdbDevice> devices();
private:
    QTimer *deviceWatchTimer;
};

#endif // ADBTRACE_H
