#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <map>
#include <utility>
#include <memory>

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

class AdbDevice
{
public:
    QString devId;
    QString model;
    QString displayName;
    QString vendor;

    bool isEmpty() const;
};

struct AdbGlobal;

class AdbShell
{
public:
    AdbShell(const QString &deviceId = {});
    AdbShell(const AdbShell &) = delete;
    ~AdbShell();

    bool connect(const QString &deviceId);
    bool isConnect();
    inline void disconnect(){
        exit();
    }
    std::pair<bool, QString> commandQueueWait(const QStringList &args);
    int commandQueueAsync(const QStringList &args);
    std::pair<bool, QString> commandResult(int requestId, bool waitResult = true);
    QString getprop(const QString &propname);
    bool reConnect();
    void exit();

private:
    bool hasReqID(int requestId);

    std::shared_ptr<AdbGlobal> ref;
};

class Adb : public QObject
{
    Q_OBJECT

public:
    AdbDevice device;
    QList<AdbDevice> cachedDevices;

    Adb(QObject * parent = nullptr);
    virtual ~Adb();

    AdbConStatus status() const;
    bool isConnected();
    void connectFirst();
    void connect(const QString& devId);
    std::pair<bool, std::unique_ptr<AdbShell>> runShell();
    void disconnect();
    static void startServer();
    static void killServer();
    static AdbDevice getDevice(const QString& deviceSerial);
    static QList<PackageIO> getPackages(const QString& deviceSerial);
    static void killPackages(const QString &deviceSerial, const QList<PackageIO> &packages, int& successCount);
    static bool uninstallPackages(const QString &deviceSerial, const QStringList& packages, int& successCount);
    static bool disablePackages(const QString &deviceSerial, const QStringList& packages, int& successCount);
    static bool enablePackages(const QString &deviceSerial, const QStringList& packages, int& successCount);

signals:
    void onDeviceChanged(const AdbDevice& device, AdbConState state);

private slots:
    void onDeviceWatch();

public:
    static AdbConStatus deviceStatus(const QString& deviceId);
    static AdbConStatus deviceStatus(const AdbDevice &device);
    static QList<AdbDevice> getDevices();
    static uint deviceHash(const AdbDevice& device);

private:
    QTimer *deviceWatchTimer;
};

bool operator ==(const AdbDevice& lhs, const AdbDevice& rhs);

QString AdbExecutableFilename();
