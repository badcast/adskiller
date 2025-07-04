#include <limits>

#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QRandomGenerator>

#include "adbfront.h"


AdbShell::AdbShell(const QString &deviceId) : thread(nullptr)
{
    connect(deviceId);
}

AdbShell::~AdbShell() { exit(); }

bool AdbShell::connect(const QString &deviceId)
{
    if (deviceId.isEmpty() || isConnect() || Adb::deviceStatus(deviceId) == AdbConStatus::UNKNOWN)
        return false;

    std::function<void(void)> __waitshell__ = [this](void)
    {
        QProcess process;
        QStringList _args;
        QString fullArgs;
        QByteArray output;
        std::unordered_map<int,QStringList>::iterator iter;
        process.start(ADBExecFilePath(), QStringList() << "-s" << this->deviceId << "shell", QIODevice::ReadWrite);

        if(process.waitForStarted())
        {
            this->data = 1;
            while(isConnect() && process.state() == QProcess::ProcessState::Running)
            {
                mutex.lock();
                iter = std::begin(requests);
                if(iter != std::end(requests))
                {
                    int reqId = iter->first;
                    _args = iter->second;
                    mutex.unlock();

                    fullArgs = std::move(_args.join(' '));
                    fullArgs += "\n";
                    process.write(fullArgs.toUtf8());
                    process.waitForBytesWritten();
                    process.waitForReadyRead(10000);
                    output = process.readAllStandardOutput();

                    if(process.state() != QProcess::ProcessState::Running)
                    {
                        break;
                    }

#ifdef WIN32
                    if(!output.isEmpty())
                        output.replace("\r\n", "\n");
#endif
                    if(!output.isEmpty())
                        output.remove(output.length()-1, 1);

                    mutex.lock();
                    responces[reqId] = output;
                    output.clear();
                    // erase after use.
                    requests.erase(reqId);
                }
                mutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            process.close();
        }
        else
        {
            qDebug() << "fail start adb shell";
        }
        this->data = 0;
    };

    requests.clear();
    responces.clear();
    this->deviceId = deviceId;
    if(thread != nullptr)
        delete thread;
    thread = new std::thread(__waitshell__);
    return thread != nullptr;
}

bool AdbShell::isConnect()
{
    return !deviceId.isEmpty() && Adb::deviceStatus(deviceId) == AdbConStatus::DEVICE && data > 0;
}

std::pair<bool, QString> AdbShell::commandQueueWait(const QStringList &args) {

    int reqId = commandQueueAsync(args);
    std::pair<bool,QString> result;
    if(reqId != -1)
        result = commandResult(reqId, true);
    return result;
}

int AdbShell::commandQueueAsync(const QStringList &args)
{
    int reqId;
    if(args.empty() || !isConnect())
        return -1;
    mutex.lock();
    do
    {
        reqId = QRandomGenerator::global()->bounded(0, std::numeric_limits<int>::max());
    }
    while (requests.find(reqId) != std::end(requests));
    requests[reqId] = args;
    mutex.unlock();
    return reqId;
}

std::pair<bool, QString> AdbShell::commandResult(int requestId, bool waitResult)
{
    bool found;
    QString output {};
    std::unordered_map<int,QString>::iterator iter;
    if((found = hasReqID(requestId)))
    {
        do
        {
            if(waitResult)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            mutex.lock();
            if((found=((iter=responces.find(requestId)) != std::end(responces))))
            {
                output = iter->second;
                // erase after use.
                responces.erase(iter);
            }
            mutex.unlock();
        }
        while(isConnect() && waitResult && !found);
    }
    return {found, output};
}

bool AdbShell::hasReqID(int requestId)
{
    bool found;
    mutex.lock();
    found = (requests.find(requestId) != std::end(requests)) || (responces.find(requestId) != std::end(responces));
    mutex.unlock();
    return found;
}

QString AdbShell::getprop(const QString &propname)
{
    return commandQueueWait(QStringList() << "getprop" << propname).second;
}

void AdbShell::exit()
{
    if (thread == nullptr)
        return;
    commandQueueWait(QStringList() << "exit" << ";" << "echo 1");
    deviceId.clear();
    thread->join();
    for(const std::pair<int, QStringList> & q : std::as_const(requests))
    {
        responces[q.first] = QString{};
    }
    requests.clear();
    delete thread;
    thread = nullptr;
}
