#pragma once

#include <memory>
#include <functional>

#include <QByteArray>
#include <QString>
#include <QVariant>

#include "begin.h"
#include "adbfront.h"

enum DeviceConnectType
{
    ADB
};

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

enum TaskType
{
    Unknown,
    AdsKiller
};

class Worker;
class TaskManager;
class Task;

class CipherAlgoCrypto
{
private:
    static QByteArray ConvBytes(const QByteArray &bytes, const QByteArray &key);

public:
    static QString PackDC(const QByteArray &dataInit, const QByteArray &key);
    static QByteArray UnpackDC(const QString &packed);
    static QByteArray RandomKey();
};

class Task
{
    friend class Worker;
public:
    using Invoker = std::function<bool(void)>;
    using InvokerA1 = std::function<bool(QVariant)>;
    using Checker = std::function<int(void)>;

    Task(Invoker invoker, Invoker deInvoker, Checker checker);
    Task(InvokerA1 invokerWithParam, Invoker deInvoker, Checker checker);
    ~Task() = default;

    inline bool isNone();
    inline bool isRunning();
    inline bool isFinish();
    bool run(QVariant arg = 0);
    void kill();

    static std::shared_ptr<Task> CreateTask(TaskType type);

private:
    Invoker _invoker0;
    InvokerA1 _invoker1;
    Invoker _deinvoker;
    Checker _checker;
};

class Worker : public QObject
{
    Q_OBJECT

private:
    DeviceConnectType mDeviceType;
    AdbDevice mAdbDevice;
    std::shared_ptr<Task> mTask;

private slots:
    void onAdbDeviceConnect(const AdbDevice& adbDevice);

public:
    Worker(DeviceConnectType deviceType, std::shared_ptr<Task> task, QObject * parent = nullptr) : QObject(parent), mDeviceType(deviceType), mTask(task) {}

    bool isStarted();
    bool canStart();
    bool start();
    void stop();

    DeviceConnectType deviceType() const
    {
        return mDeviceType;
    }

    static std::shared_ptr<Worker> CreateAdskillService();
};

