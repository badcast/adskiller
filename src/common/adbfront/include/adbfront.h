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
    QString marketingName;

    AdbDevice() : devId(), model(), displayName(), vendor() {}

    bool isEmpty() const;
};

struct AdbGlobal;
class AdbShell;
class AdbFileIO;
class AdbSysInfo;

class AdbShell
{
public:
    AdbShell(const QString &deviceId = {});
    AdbShell(const AdbShell &) = delete;
    virtual ~AdbShell();

    bool connect(const QString &deviceId);
    bool isConnect();
    inline void disconnect(){
        exit();
    }
    std::pair<bool, QString> commandQueueWait(const QStringList &args);
    template<typename... Args>
    inline std::pair<bool, QString> commandQueueWaits(Args&&... args) {
        return commandQueueWait((QStringList() << ... << std::forward<Args>(args)));
    }
    int commandQueueAsync(const QStringList &args);
    template<typename... Args>
    inline int commandQueueAsyncs(Args&&... args) {
        return commandQueueAsync((QStringList() << ... << std::forward<Args>(args)));
    }
    std::pair<bool, QString> commandResult(int requestId, bool waitResult = true);
    QString getprop(const QString &propname);
    bool reConnect();
    void exit();
    std::shared_ptr<AdbSysInfo> getInfo();
    AdbFileIO getFileIO();

private:
    bool hasReqID(int requestId);

    std::shared_ptr<AdbGlobal> ref;
};

class AdbSysInfo
{
public:
    QString systemName;
    QString kernelReleaseVersion;
    QString kernelVersion;
    QString machine;
    std::uint32_t osVersion;
    std::int64_t diskTotal;
    std::int64_t diskUsed;
    std::int64_t ramTotal;
    std::int64_t ramUsed;
    std::int64_t swapTotal;
    std::int64_t swapUsed;
    bool swapEnabled;
    bool isAndroid;

    AdbSysInfo() :
        diskTotal(-1),
        diskUsed(-1),
        ramTotal(-1),
        ramUsed(-1),
        swapTotal(-1),
        swapUsed(-1),
        osVersion(0),
        swapEnabled(false)    {}

    QString OSVersionString() const;
    //TODO: make design capacity
    QString StorageDesignString() const;
    QString RAMDesignString() const;
};

class AdbFileIO : public AdbShell
{
public:
    AdbFileIO(const QString& deviceId = {});
    AdbFileIO(const AdbFileIO&) = delete;

    bool exists(const QString& filePath);
    bool write(const QString& filePath, const QByteArray & buffer);
    QByteArray read(const QString& filePath);
    bool deleteFile(const QString& filePath);
    QStringList getFiles(const QString& dirPath, bool includeDirs = true);
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
