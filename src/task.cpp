#include <memory>

#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QString>

#include "extension.h"
#include "AntiMalware.h"
#include "mainwindow.h"

Task::Task(Task::Invoker invoker, Invoker deInvoker, Checker checker) : _invoker0(invoker), _invoker1(nullptr), _deinvoker(deInvoker), _checker(checker)
{}
Task::Task(Task::InvokerA1 invokerWithParam, Invoker deInvoker, Checker checker) : _invoker0(nullptr), _invoker1(invokerWithParam), _deinvoker(deInvoker), _checker(checker)
{}

bool Task::isNone()
{
    return _checker() == 0;
}

bool Task::isRunning()
{
    return _checker() == 1;
}

bool Task::isFinish()
{
    return _checker() == 2;
}

bool Task::run(QVariant arg)
{
    if(!isRunning() || !_invoker0 || !_invoker1)
        return false;
    return _invoker0 ? _invoker0() : (_invoker1 ? _invoker1(arg) : false);
}

void Task::kill()
{
    if(_deinvoker)
        _deinvoker();
}

std::shared_ptr<Task> Task::CreateTask(TaskType type)
{
    std::shared_ptr<Task> _task;
    switch(type)
    {
    case TaskType::AdsKiller:
    {
        _task = std::make_shared<Task>([](QVariant deviceSerial)->bool{malwareStart(deviceSerial.toString()); return malwareStatus() == Running; }, [](void)->bool {malwareKill(); return true; }, [](void)->int
                                       {
                                           MalwareStatus ms = malwareStatus();
                                           if(ms == Idle)
                                               return 0;
                                           if(ms == Running)
                                               return 1;
                                           if(ms == Error)
                                               return 2;
                                       });
        break;
    }
    default:
        _task = nullptr;
        break;
    }
    return _task;
}

void Worker::onAdbDeviceConnect(const AdbDevice &adbDevice)
{
    mAdbDevice = adbDevice;
}

bool Worker::isStarted()
{
    return mTask && mTask->isRunning();
}

bool Worker::canStart()
{
    return mTask && mTask->isNone() && (mDeviceType == ADB && Adb::deviceStatus(mAdbDevice.devId) == DEVICE);
}

bool Worker::start()
{
    if(!canStart())
        return false;
    return mTask->run();
}

void Worker::stop()
{
    if(isStarted())
        mTask->kill();
}

std::shared_ptr<Worker> Worker::CreateAdskillService()
{
    std::shared_ptr<Worker> worker = std::make_shared<Worker>(ADB, Task::CreateTask(TaskType::AdsKiller));
    QObject::connect(MainWindow::current, &MainWindow::AdbDeviceConnected, &(*worker), &Worker::onAdbDeviceConnect);
    return worker;
}
