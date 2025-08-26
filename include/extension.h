#pragma once

#include <functional>

#include <QByteArray>
#include <QString>

#include "begin.h"
#include "adbfront.h"

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

enum TaskType
{
    AdsKiller
};

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
    friend class TaskManager;
public:
    using Invoker = std::function<bool(void)>;
    using Checker = std::function<int(void)>;

    Task(Invoker invoker, Invoker deInvoker, Checker checker);
    ~Task() = default;

    inline bool isNone();
    inline bool isRunning();
    inline bool isFinish();
    bool run();
    void kill();

private:
    Invoker _invoker;
    Invoker _deinvoker;
    Checker _checker;
};

class TaskManager
{
public:
    std::unique_ptr<Task> CreateTask(TaskType type);
};
