#include <memory>

#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QString>

#include "extension.h"
#include "AntiMalware.h"
#include "mainwindow.h"

QByteArray CipherAlgoCrypto::RandomKey()
{
    int x;
    QByteArray key;
    key.resize(8);
    for (x = 0; x < key.length(); ++x)
    {
        key[x] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return key;
}

QByteArray CipherAlgoCrypto::ConvBytes(const QByteArray &bytes, const QByteArray &key)
{
    int x;
    QByteArray retval;
    retval.resize(bytes.length());
    for (x = 0; x < bytes.length() && !key.isEmpty(); ++x)
        retval[x] = bytes[x] ^ key[x % key.length()];
    return retval;
}

QString CipherAlgoCrypto::PackDC(const QByteArray &dataInit, const QByteArray &key)
{
    QByteArray retval{};
    QByteArray data = ConvBytes(dataInit, key);
    int keylen = key.toHex().length();
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    retval.resize(hashlen + data.length() + keylen + 1);
    retval[hashlen] = static_cast<char>(keylen);
    retval.replace(hashlen + 1, keylen, key.toHex());
    retval.replace(hashlen + 1 + keylen, data.length(), data);
    retval.replace(0, hashlen, QCryptographicHash::hash(retval.mid(hashlen), QCryptographicHash::Sha256));
    return QLatin1String(retval.toBase64());
}

QByteArray CipherAlgoCrypto::UnpackDC(const QString &packed)
{
    QByteArray key{}, data{};
    int keylen;
    int hashlen = QCryptographicHash::hashLength(QCryptographicHash::Sha256);
    data = QByteArray::fromBase64(packed.toLatin1());
    if (!data.isEmpty() && data.mid(0, hashlen) == QCryptographicHash::hash(data.mid(hashlen), QCryptographicHash::Sha256))
    {
        keylen = data[hashlen];
        key = QByteArray::fromHex(data.mid(hashlen + 1, keylen));
        data = ConvBytes(data.mid(hashlen + 1 + keylen), key);
    }
    else
    {
        data.clear();
    }
    return data;
}

Task::Task(Task::Invoker invoker, Invoker deInvoker, Checker checker) : _invoker(invoker), _deinvoker(deInvoker), _checker(checker)
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

bool Task::run()
{
    if(!_invoker || !isRunning())
        return false;
    return _invoker();
}

void Task::kill()
{
    if(_deinvoker)
        _deinvoker();
}

std::shared_ptr<Task> Task::CreateTask(TaskType type, const QString& deviceSerial)
{
    std::shared_ptr<Task> _task;
    switch(type)
    {
    case TaskType::AdsKiller:
    {
        _task = std::make_shared<Task>([deviceSerial](void)->bool{malwareStart(deviceSerial); return malwareStatus() == Running; }, [](void)->bool {malwareKill(); return true; }, [](void)->int
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
    std::shared_ptr<Worker> worker;

    if(MainWindow::current->startDeviceConnect(ADB))
    {
        worker = std::make_shared<Worker>(ADB, Task::CreateTask(TaskType::AdsKiller, {}));
        QObject::connect(MainWindow::current, &MainWindow::AdbDeviceConnected, &(*worker), &Worker::onAdbDeviceConnect);
    }

    return worker;
}
