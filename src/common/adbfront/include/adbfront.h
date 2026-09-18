#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <QByteArray>
#include <QFileInfo>
#include <QList>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
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

struct AdbPackageInfo
{
    QString packageName;
    QString appName;
    QString apkPath;
    qint64 apkSize = 0;
    bool isSystem = false;
    bool isDisabled = false;
    QString versionName = "N/a";
};

struct AdbPackageDetails : public AdbPackageInfo
{
    QString versionCode = "N/a";
    QString minSdk = "N/a";
    QString targetSdk = "N/a";
    QString codePath = "N/a";
    QString dataDir = "N/a";
    QString installer = "N/a";
    QString firstInstallTime = "N/a";
    QString lastUpdateTime = "N/a";
    QString primaryCpuAbi = "N/a";
    QString mainActivity = "N/a";
    QString signatures = "N/a";
    QStringList requestedPermissions;
    QSet<QString> grantedPermissions;
    QStringList activities;
    QStringList services;
    QStringList receivers;
    QStringList providers;
    QString rawDumpsys;
};

class AdbDevice
{
public:
    QString devId;
    QString model;
    QString displayName;
    QString vendor;
    QString marketingName;

    AdbDevice() : devId(), model(), displayName(), vendor()
    {
    }

    bool isEmpty() const;
};

struct AdbGlobal;
class AdbShell;
class AdbFileIO;
class AdbSysInfo;

struct AdbFileInfo
{
    QString name;
    QString fullPath;
    qint64 size = 0;
    bool isDir = false;
    QString permissions;
    QString modifyTime;
};

class AdbShell
{
public:
    AdbShell(const QString &deviceId = {});
    AdbShell(const AdbShell &) = delete;
    AdbShell(AdbShell &&other) noexcept;
    AdbShell &operator=(AdbShell &&other) noexcept;
    virtual ~AdbShell();

    bool connect(const QString &deviceId);
    bool isConnect();
    inline void disconnect()
    {
        exit();
    }
    std::pair<bool, QString> commandQueueWait(const QStringList &args);
    template <typename... Args>
    inline std::pair<bool, QString> commandQueueWaits(Args &&...args)
    {
        return commandQueueWait((QStringList() << ... << std::forward<Args>(args)));
    }
    int commandQueueAsync(const QStringList &args);
    template <typename... Args>
    inline int commandQueueAsyncs(Args &&...args)
    {
        return commandQueueAsync((QStringList() << ... << std::forward<Args>(args)));
    }
    std::pair<bool, QString> commandResult(int requestId, bool waitResult = true);
    QString getprop(const QString &propname);
    bool reConnect();
    void exit();
    std::shared_ptr<AdbSysInfo> getInfo();
    AdbFileIO getFileIO();
    QString deviceId() const;

    // Package and Application management
    QList<AdbPackageInfo> getPackageList();
    AdbPackageDetails getPackageDetails(const QString &packageName);
    static AdbPackageDetails parseDumpsys(const QString &dumpsysOutput, const QString &packageName);
    static QString sdkToAndroidVersion(int sdk);
    static QString friendlyAppName(const QString &pkgName);
    QByteArray extractPackageIconData(const QString &apkPath, const QString &packageName = {});

    bool launchPackage(const QString &packageName);
    bool stopPackage(const QString &packageName);
    bool setPackageEnabled(const QString &packageName, bool enabled);
    bool enablePackage(const QString &packageName);
    bool disablePackage(const QString &packageName);
    bool clearPackageData(const QString &packageName);
    bool uninstallPackage(const QString &packageName, bool keepData = false);
    std::pair<bool, QString> installPackage(const QString &localApkPath, bool reinstall = true);

    // File transfer
    bool pullFile(const QString &remotePath, const QString &localPath);
    bool pushFile(const QString &localPath, const QString &remotePath);

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

    AdbSysInfo() : diskTotal(-1), diskUsed(-1), ramTotal(-1), ramUsed(-1), swapTotal(-1), swapUsed(-1), osVersion(0), swapEnabled(false)
    {
    }

    QString OSVersionString() const;
    QString StorageDesignString() const;
    QString RAMDesignString() const;
};

class AdbFileIO : public AdbShell
{
public:
    AdbFileIO(const QString &deviceId = {});
    AdbFileIO(const AdbFileIO &) = delete;
    AdbFileIO(AdbFileIO &&other) noexcept;
    AdbFileIO &operator=(AdbFileIO &&other) noexcept;
    ~AdbFileIO() override = default;

    bool exists(const QString &filePath);
    bool write(const QString &filePath, const QByteArray &buffer);
    QByteArray read(const QString &filePath);
    bool deleteFile(const QString &filePath);
    QStringList getFiles(const QString &dirPath, bool includeDirs = true);

    QList<AdbFileInfo> getFileList(const QString &dirPath);
    bool makeDir(const QString &dirPath);
};

class Adb : public QObject
{
    Q_OBJECT

public:
    AdbDevice device;
    QList<AdbDevice> cachedDevices;

    Adb(QObject *parent = nullptr);
    virtual ~Adb();

    AdbConStatus status() const;
    bool isConnected();
    void connectFirst();
    void connect(const QString &devId);
    std::pair<bool, std::unique_ptr<AdbShell>> runShell();
    void disconnect();
    static void startServer();
    static void killServer();
    static AdbDevice getDevice(const QString &deviceSerial);
    static QList<PackageIO> getPackages(const QString &deviceSerial);
    static void killPackages(const QString &deviceSerial, const QList<PackageIO> &packages, int &successCount);
    static bool uninstallPackages(const QString &deviceSerial, const QStringList &packages, int &successCount);
    static bool disablePackages(const QString &deviceSerial, const QStringList &packages, int &successCount);
    static bool enablePackages(const QString &deviceSerial, const QStringList &packages, int &successCount);

signals:
    void onDeviceChanged(const AdbDevice &device, AdbConState state);

private slots:
    void onDeviceWatch();

public:
    static AdbConStatus deviceStatus(const QString &deviceId);
    static AdbConStatus deviceStatus(const AdbDevice &device);
    static QList<AdbDevice> getDevices();
    static uint deviceHash(const AdbDevice &device);

private:
    QTimer *deviceWatchTimer;
};

std::pair<bool, QString> adb_send_cmd(int &exitCode, const QStringList &arguments);
std::pair<bool, QString> adb_send_cmd(const QStringList &arguments);

bool operator==(const AdbDevice &lhs, const AdbDevice &rhs);

QString AdbExecutableFilename();
