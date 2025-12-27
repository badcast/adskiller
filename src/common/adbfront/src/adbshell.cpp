#include <limits>

#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QRandomGenerator>

#include "adbcmds.h"
#include "adbfront.h"

struct AdbGlobal
{
    std::unordered_map<int,QStringList> requests;
    std::unordered_map<int,QString> responces;
    std::thread *thread;
    std::mutex mutex;
    std::pair<std::uint32_t,std::uint32_t> dataRxTx;
    QString devId;
    int data;
    int ref;
};

QHash<QString, std::shared_ptr<AdbGlobal>> globals;

extern std::pair<bool, QString> adb_send_cmd(int &exitCode, const QStringList &arguments);

/*
void IOInterpret_procby(AdbGlobal * global)
{
    std::unordered_map<int,QStringList>::iterator iter;
    global->data = 1;
    while(global->data > 0 && global->devId.isEmpty() && Adb::deviceStatus(global->devId) == AdbConStatus::DEVICE)
    {
        global->mutex.lock();
        iter = std::begin(global->requests);
        if(iter != std::end(global->requests))
        {
            int exitCode;
            QProcess process;
            QString retval;
            process.start(AdbExecutableFilename(), arguments);
            if (!process.waitForFinished(10000))
            {
                qDebug() << "ADB Failed";
                process.kill();
                process.waitForFinished(1000);
                return {false, {}};
            }
            retval = process.readAllStandardOutput();
            exitCode = process.exitCode();
#ifdef WIN32
            retval.replace("\r\n", "\n");
#endif
            if(ret.first)
            {

            }

            global->requests.erase(iter);
        }
        global->mutex.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    global->data = 0;
}
*/

void IOInterpret_tty(AdbGlobal * global)
{
    QProcess process;
    QStringList _args;
    QString fullArgs;
    QByteArray output;
    QByteArray session;
    std::unordered_map<int,QStringList>::iterator iter;
    int lastIndex,len,reqId, retCode;
    if(global == nullptr)
        return;
    process.start(AdbExecutableFilename(), QStringList() << "-s" << global->devId << "shell", QIODevice::ReadWrite);
    if(process.waitForStarted())
    {
        global->data = 1;
        while(!global->devId.isEmpty() && Adb::deviceStatus(global->devId) == AdbConStatus::DEVICE && global->data > 0 && process.state() == QProcess::ProcessState::Running)
        {
            global->mutex.lock();
            iter = std::begin(global->requests);
            if(iter != std::end(global->requests))
            {
                reqId = iter->first;
                _args = iter->second;
                global->mutex.unlock();

                fullArgs = std::move(_args.join(' '));
                fullArgs += "; echo \"|$?\"\n";

#ifdef WIN32
                fullArgs.replace('\n', "\r\n");
#endif
                session = std::move(fullArgs.toUtf8());
                global->dataRxTx.second += static_cast<std::uint32_t>(session.size());
                process.write(session);
                process.waitForBytesWritten(ExecWaitTime);

                do
                {
                    process.waitForReadyRead(ExecWaitTime);
                    session = std::move(process.readAllStandardOutput());
                    if(session.startsWith("shell@"))
                    {
                        int i = session.indexOf('$', 7)+1;
                        session = session.mid(i, session.length()-i).trimmed();
                        // End of bug.
                        // BUG: old version android (4 and less) has bug IO/TTY;
                        lastIndex = -1;
                        process.close();
                        break;
                    }

                    output += session;
                    global->dataRxTx.first += static_cast<std::uint32_t>(session.size());
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
                    session = std::move(output.mid(lastIndex+1, (output.length()-lastIndex+1)));
                    retCode = session.toInt();
                    (void)retCode;

                    if(lastIndex > 0)
                        lastIndex--;
                    len = 1 + output.length()-lastIndex;

                    output.remove(lastIndex, len);
                }

                global->mutex.lock();
                global->responces[reqId] = std::move(output);
                output.clear();
                // erase after use.
                global->requests.erase(reqId);
            }
            global->mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        process.close();
    }
    else
    {
        qDebug() << "fail start adb shell";
    }
    global->data = 0;
}

std::shared_ptr<AdbGlobal> refGlobal(const QString& devId)
{
    std::shared_ptr<AdbGlobal> glob;

    if(!globals.contains(devId))
    {
        glob = std::make_shared<AdbGlobal>();
        globals.insert(devId, glob);
        glob->ref = 1;
        glob->dataRxTx = {};
        glob->data = 0;
        glob->devId = devId;
        glob->requests.clear();
        glob->responces.clear();
        glob->thread = new std::thread(IOInterpret_tty, glob.get());
        int counter = 5;
        while(glob->data == 0 && counter-- > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else
    {
        glob = globals[devId];
        glob->ref++;
    }
    return glob;
}

void unrefGlobal(std::shared_ptr<AdbGlobal> global)
{
    if(global == nullptr || --global->ref > 0)
        return;

    QString devId = global->devId;
    global->mutex.lock();
    global->data = 0;
    global->mutex.unlock();
    // commandQueueWait(QStringList() << "exit");
    global->thread->join();
    delete global->thread;
    global->thread = nullptr;
    for(const std::pair<int, QStringList> & q : std::as_const(global->requests))
        global->responces[q.first] = QString{};
    globals.remove(devId);
}

AdbShell::AdbShell(const QString &deviceId) : ref(nullptr)
{
    connect(deviceId);
}

AdbShell::~AdbShell() { exit(); }

bool AdbShell::connect(const QString &deviceId)
{
    if (deviceId.isEmpty() || isConnect() || Adb::deviceStatus(deviceId) != AdbConStatus::DEVICE)
        return false;
    return (ref = refGlobal(deviceId)) != nullptr;
}

bool AdbShell::isConnect()
{
    return ref && !ref->devId.isEmpty() && Adb::deviceStatus(ref->devId) == AdbConStatus::DEVICE && ref->data > 0;
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
    ref->mutex.lock();
    do
    {
        reqId = QRandomGenerator::global()->bounded(0, std::numeric_limits<int>::max());
    }
    while (ref->requests.find(reqId) != std::end(ref->requests));
    ref->requests[reqId] = args;
    ref->mutex.unlock();
    return reqId;
}

std::pair<bool, QString> AdbShell::commandResult(int requestId, bool waitResult)
{
    bool found;
    QString output {};
    std::unordered_map<int,QString>::iterator iter;
    if((found = isConnect() && hasReqID(requestId)))
    {
        do
        {
            if(waitResult)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            ref->mutex.lock();
            if((found=((iter=ref->responces.find(requestId)) != std::end(ref->responces))))
            {
                output = iter->second;
                // erase after use.
                ref->responces.erase(iter);
            }
            ref->mutex.unlock();
        }
        while(isConnect() && waitResult && !found);
    }
    return {found, output};
}

bool AdbShell::hasReqID(int requestId)
{
    bool found;
    ref->mutex.lock();
    found = (ref->requests.find(requestId) != std::end(ref->requests)) || (ref->responces.find(requestId) != std::end(ref->responces));
    ref->mutex.unlock();
    return found;
}

QString AdbShell::getprop(const QString &propname)
{
    return commandQueueWaits("getprop", propname).second;
}

bool AdbShell::reConnect()
{
    if(ref == nullptr)
        return false;
    QString devId = ref->devId;
    if(isConnect())
        exit();
    return connect(devId);
}

void AdbShell::exit()
{
    unrefGlobal(ref);
    ref.reset();
}
