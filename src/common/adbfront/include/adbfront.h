#ifndef ADBTRACE_H
#define ADBTRACE_H

#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <map>
#include <utility>

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


class AdbShell
{
public:
    AdbShell(const QString &deviceId = {});
    AdbShell(const AdbShell &) = delete;
    ~AdbShell();

    bool connect(const QString &deviceId);
    bool isConnect();
    std::pair<bool, QString> commandQueueWait(const QStringList &args);
    int commandQueueAsync(const QStringList &args);
    std::pair<bool, QString> commandResult(int requestId, bool waitResult = true);
    bool hasCommandRequest(int requestId);
    QString getprop(const QString &propname);
    void reConnect();
    void exit();

private:
    QString deviceId;
    std::unordered_map<int,QStringList> requests;
    std::unordered_map<int,QString> responces;
    std::thread *thread;
    std::mutex mutex;
    int data;
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
    static AdbConStatus deviceStatus(const QString& deviceId);
    static AdbConStatus deviceStatus(const AdbDevice &device);
    static QList<AdbDevice> getDevices();
    static uint deviceHash(const AdbDevice& device);

private:
    QTimer *deviceWatchTimer;
};

bool operator ==(const AdbDevice& lhs, const AdbDevice& rhs);

QString ADBExecFilePath();

#endif // ADBTRACE_H
