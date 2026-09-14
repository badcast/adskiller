#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <deque>
#include <unordered_map>
#include <atomic>

#include <QByteArray>
#include <QFileInfo>
#include <QList>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QHash>
#include <QObject>

enum AppleConStatus
{
    APPLE_UNKNOWN = 0,
    APPLE_UNAUTH = 1,
    APPLE_DEVICE = 2,
    APPLE_RECOVERY = 3,
    APPLE_DFU = 4
};

enum AppleConState
{
    AppleRemoved,
    AppleAdd
};

enum class AppleDeviceMode
{
    Normal,
    Recovery,
    DFU,
    Unknown
};

class AppleDevice
{
public:
    QString devId;              // UDID in normal mode, ECID in recovery/DFU mode
    QString model;              // ProductType, e.g. "iPhone14,5"
    QString displayName;        // e.g. "iPhone 13 (User)"
    QString vendor;             // "Apple"
    QString marketingName;      // e.g. "iPhone 13"
    QString productVersion;     // e.g. "17.5.1"
    QString buildVersion;       // e.g. "21F90"
    QString serialNumber;       // Serial number
    QString ecid;               // ECID / UniqueChipID
    AppleDeviceMode mode;       // Normal, Recovery, DFU
    bool isPaired;              // Host pairing trusted

    AppleDevice()
        : devId(), model(), displayName(), vendor("Apple"),
          marketingName(), productVersion(), buildVersion(),
          serialNumber(), ecid(), mode(AppleDeviceMode::Unknown),
          isPaired(false)
    {
    }

    bool isEmpty() const;
    QString modeString() const;
};

bool operator==(const AppleDevice &lhs, const AppleDevice &rhs);

class AppleSysInfo
{
public:
    QString systemName;
    QString productType;
    QString marketingName;
    QString productVersion;
    QString buildVersion;
    QString deviceName;
    QString serialNumber;
    QString uniqueChipID;
    QString cpuArchitecture;
    QString wifiAddress;
    QString bluetoothAddress;
    std::int64_t diskTotal;
    std::int64_t diskUsed;
    int batteryLevel;
    bool isCharging;
    QString activationState;
    bool passwordProtected;

    AppleSysInfo()
        : diskTotal(-1), diskUsed(-1), batteryLevel(-1),
          isCharging(false), passwordProtected(false)
    {
    }

    QString OSVersionString() const;
    QString StorageDesignString() const;
    QString BatteryDesignString() const;
};

struct AppleGlobal;

class AppleShell
{
public:
    AppleShell(const QString &deviceId = {});
    AppleShell(const AppleShell &) = delete;
    AppleShell(AppleShell &&other) noexcept;
    AppleShell &operator=(AppleShell &&other) noexcept;
    virtual ~AppleShell();

    bool connect(const QString &deviceId);
    bool isConnect() const;
    inline void disconnect()
    {
        exit();
    }
    void exit();
    bool reConnect();
    QString deviceId() const;

    // Command execution
    std::pair<bool, QString> commandQueueWait(const QString &program, const QStringList &args);
    template <typename... Args>
    inline std::pair<bool, QString> commandQueueWaits(const QString &program, Args &&...args)
    {
        return commandQueueWait(program, (QStringList() << ... << std::forward<Args>(args)));
    }
    int commandQueueAsync(const QString &program, const QStringList &args);
    template <typename... Args>
    inline int commandQueueAsyncs(const QString &program, Args &&...args)
    {
        return commandQueueAsync(program, (QStringList() << ... << std::forward<Args>(args)));
    }
    std::pair<bool, QString> commandResult(int requestId, bool waitResult = true);

    // Apple properties & diagnostics
    QString getprop(const QString &propname, const QString &domain = {});
    std::shared_ptr<AppleSysInfo> getInfo();
    bool validatePairing();
    bool requestPairing();
    bool enterRecovery();
    bool exitRecovery();
    bool restartDevice();
    bool shutdownDevice();

private:
    std::shared_ptr<AppleGlobal> ref;
};

class Apple : public QObject
{
    Q_OBJECT

public:
    AppleDevice device;
    QList<AppleDevice> cachedDevices;

    Apple(QObject *parent = nullptr);
    virtual ~Apple();

    AppleConStatus status() const;
    bool isConnected();
    void connectFirst();
    void connect(const QString &devId);
    std::pair<bool, std::unique_ptr<AppleShell>> runShell();
    void disconnect();

    static AppleDevice getDevice(const QString &deviceId);
    static AppleConStatus deviceStatus(const QString &deviceId);
    static AppleConStatus deviceStatus(const AppleDevice &device);
    static QList<AppleDevice> getDevices();
    static uint deviceHash(const AppleDevice &device);
    static QString marketingNameForModel(const QString &model);

signals:
    void onDeviceChanged(const AppleDevice &device, AppleConState state);

private slots:
    void onDeviceWatch();

private:
    QTimer *deviceWatchTimer;
};

std::pair<bool, QString> apple_send_cmd(int &exitCode, const QString &program, const QStringList &arguments);
std::pair<bool, QString> apple_send_cmd(const QString &program, const QStringList &arguments);
QString AppleExecutableFilename(const QString &program);
