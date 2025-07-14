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
                    int lastIndex;
                    int len;
                    int reqId = iter->first;
                    int retCode;
                    _args = iter->second;
                    mutex.unlock();

                    fullArgs = std::move(_args.join(' '));
                    fullArgs += "\necho \"|$?\"\n";

#ifdef WIN32
                    fullArgs.replace('\n', "\r\n");
#endif

                    process.write(fullArgs.toUtf8());
                    process.waitForBytesWritten();

                    do
                    {
                        process.waitForReadyRead(10000);
                        output += process.readAllStandardOutput();
#ifdef WIN32
                        output.replace("\r\n", "\n");
#endif
                        lastIndex = output.lastIndexOf('|');
                    }
                    while(lastIndex == -1 && process.state() == QProcess::ProcessState::Running);

                    if(lastIndex == -1 || process.state() != QProcess::ProcessState::Running)
                    {
                        break;
                    }

                    if(!output.isEmpty())
                    {
                        len = 1;

                        QByteArray dupNum = output.mid(lastIndex+1, (output.length()-lastIndex+1));
                        retCode = dupNum.toInt();
                        if(lastIndex > 0)
                            lastIndex--;
                        len += output.length()-lastIndex;

                        output.remove(lastIndex, len);
                    }

                    mutex.lock();
                    responces[reqId] = std::move(output);
                    output.clear();
                    // erase after use.
                    requests.erase(reqId);
                }
                mutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

bool AdbShell::reConnect()
{
    if(isConnect())
        exit();
    return connect(deviceId);
}

void AdbShell::exit()
{
    if (thread == nullptr)
        return;
    commandQueueWait(QStringList() << "exit");
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
