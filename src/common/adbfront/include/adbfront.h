#ifndef ADBTRACE_H
#define ADBTRACE_H

#include <QList>
#include <QProcess>
#include <QTimer>

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

struct PackageIO
{
    QString applicationName;
    QString packageName;
    bool disabled;
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
    QList<AdbDevice> cachedDevices;

    Adb(QObject * parent = nullptr);
    virtual ~Adb();

    AdbConStatus status() const;
    bool isConnected();
    void connectFirst();
    void connect(const QString& devId);
    void disconnect();
    QList<PackageIO> getPackages();
    void killPackages(const QList<PackageIO> &packages, int& successCount);
    bool uninstallPackages(const QStringList& packages, int& successCount);
    bool disablePackages(const QStringList& packages, int& successCount);
    bool enablePackages(const QStringList& packages, int& successCount);

signals:
    void onDeviceChanged(const AdbDevice& device, AdbConState state);

private slots:
    void onDeviceWatch();

public:
    static AdbConStatus deviceStatus(const AdbDevice &device);
    static QList<AdbDevice> getDevices();
    static uint deviceHash(const AdbDevice& device);

private:
    QTimer *deviceWatchTimer;
};

bool operator ==(const AdbDevice& lhs, const AdbDevice& rhs);

QString ADBExecFilePath();

#endif // ADBTRACE_H
