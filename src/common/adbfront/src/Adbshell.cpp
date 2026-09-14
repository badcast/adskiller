#include <limits>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHash>
#include <QProcess>
#include <QRandomGenerator>
#include <QStringList>

#include "Adbcmds.h"
#include "Adbfront.h"

struct AdbCmdResult
{
    int exitCode = -1;
    QString output;
};

struct AdbGlobal
{
    std::deque<std::pair<int, QStringList>> requests;
    std::unordered_map<int, AdbCmdResult> responces;
    std::thread *thread = nullptr;
    std::mutex mutex;
    std::pair<std::uint32_t, std::uint32_t> dataRxTx {0, 0};
    QString devId;
    std::atomic<int> data {0};
    int ref {0};
};

static std::mutex s_globalsMutex;
static QHash<QString, std::shared_ptr<AdbGlobal>> globals;

extern std::pair<bool, QString> adb_send_cmd(int &exitCode, const QStringList &arguments);

void IOInterpret_tty(AdbGlobal *global)
{
    if(global == nullptr)
        return;

    try
    {
        QProcess process;

        process.start(AdbExecutableFilename(), QStringList() << "-s" << global->devId << "shell", QIODevice::ReadWrite);
        if(!process.waitForStarted(3000))
        {
            qWarning() << "Failed to start adb shell for device:" << global->devId;
            global->data.store(0);
            return;
        }

        global->data.store(1);

        while(!global->devId.isEmpty() && global->data.load() > 0 && process.state() == QProcess::ProcessState::Running)
        {
            int reqId = -1;
            QStringList args;

            {
                std::lock_guard<std::mutex> lock(global->mutex);
                if(!global->requests.empty())
                {
                    reqId = global->requests.front().first;
                    args = std::move(global->requests.front().second);
                    global->requests.pop_front();
                }
            }

            if(reqId != -1)
            {
                QString fullArgs = args.join(' ');
                QString marker = QString("__ADBCMD_%1_END_").arg(reqId);
                fullArgs += QString("\necho \"%1$?\"\n").arg(marker);

#ifdef WIN32
                fullArgs.replace('\n', "\r\n");
#endif
                QByteArray session = fullArgs.toUtf8();
                global->dataRxTx.second += static_cast<std::uint32_t>(session.size());

                process.write(session);
                process.waitForBytesWritten(1000);

                QString output;
                int exitCode = -1;
                bool cmdFinished = false;
                auto startTime = std::chrono::steady_clock::now();

                while(global->data.load() > 0 && process.state() == QProcess::ProcessState::Running && !cmdFinished)
                {
                    if(process.waitForReadyRead(50))
                    {
                        QByteArray chunk = process.readAllStandardOutput();
                        if(!chunk.isEmpty())
                        {
                            global->dataRxTx.first += static_cast<std::uint32_t>(chunk.size());
#ifdef WIN32
                            chunk.replace("\r\n", "\n");
#endif
                            output += QString::fromUtf8(chunk);
                        }
                    }

                    int markerIdx = output.lastIndexOf(marker);
                    if(markerIdx != -1)
                    {
                        int lineEnd = output.indexOf('\n', markerIdx + marker.length());
                        if(lineEnd != -1 || (output.length() >= markerIdx + marker.length() + 1 && (output.endsWith('\n') || output.endsWith('\r'))))
                        {
                            int codeStart = markerIdx + marker.length();
                            int codeLen = (lineEnd != -1) ? (lineEnd - codeStart) : -1;
                            QString codeStr = output.mid(codeStart, codeLen).trimmed();
                            bool ok = false;
                            exitCode = codeStr.toInt(&ok);
                            if(!ok)
                                exitCode = 0;

                            output.truncate(markerIdx);
                            if(output.endsWith('\n'))
                                output.chop(1);
                            if(output.endsWith('\r'))
                                output.chop(1);

                            cmdFinished = true;
                            break;
                        }
                    }

                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                    if(elapsedMs > ExecWaitTime)
                    {
                        qWarning() << "AdbShell command timed out after" << ExecWaitTime << "ms:" << args.join(' ');
                        process.write("\x03\n");
                        process.waitForBytesWritten(500);
                        break;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(global->mutex);
                    AdbCmdResult res;
                    res.exitCode = cmdFinished ? exitCode : -1;
                    res.output = std::move(output);
                    global->responces[reqId] = std::move(res);
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        process.close();
    }
    catch(const std::exception &e)
    {
        qWarning() << "Exception in IOInterpret_tty:" << e.what();
    }
    catch(...)
    {
        qWarning() << "Unknown exception in IOInterpret_tty";
    }

    global->data.store(0);
}

std::shared_ptr<AdbGlobal> refGlobal(const QString &devId)
{
    std::shared_ptr<AdbGlobal> glob;
    bool isNew = false;
    {
        std::lock_guard<std::mutex> lock(s_globalsMutex);
        if(!globals.contains(devId))
        {
            glob = std::make_shared<AdbGlobal>();
            glob->ref = 1;
            glob->dataRxTx = {0, 0};
            glob->data.store(0);
            glob->devId = devId;
            globals.insert(devId, glob);
            isNew = true;
        }
        else
        {
            glob = globals[devId];
            glob->ref++;
        }
    }

    if(isNew)
    {
        glob->thread = new std::thread(IOInterpret_tty, glob.get());
        int counter = 30;
        while(glob->data.load() == 0 && counter-- > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if(glob->data.load() == 0)
        {
            qWarning() << "Failed to initialize AdbShell for device:" << devId;
        }
    }
    return glob;
}

void unrefGlobal(std::shared_ptr<AdbGlobal> global)
{
    if(global == nullptr)
        return;

    QString devId;
    {
        std::lock_guard<std::mutex> lock(s_globalsMutex);
        if(--global->ref > 0)
            return;

        devId = global->devId;
        globals.remove(devId);
    }

    global->data.store(0);

    if(global->thread)
    {
        if(global->thread->joinable())
            global->thread->join();
        delete global->thread;
        global->thread = nullptr;
    }

    std::lock_guard<std::mutex> lock(global->mutex);
    global->requests.clear();
    global->responces.clear();
}

AdbShell::AdbShell(const QString &deviceId) : ref(nullptr)
{
    if(!deviceId.isEmpty())
        connect(deviceId);
}

AdbShell::AdbShell(AdbShell &&other) noexcept : ref(std::move(other.ref))
{
}

AdbShell &AdbShell::operator=(AdbShell &&other) noexcept
{
    if(this != &other)
    {
        exit();
        ref = std::move(other.ref);
    }
    return *this;
}

AdbShell::~AdbShell()
{
    exit();
}

QString AdbShell::deviceId() const
{
    return ref ? ref->devId : QString {};
}

AdbFileIO AdbShell::getFileIO()
{
    return AdbFileIO(deviceId());
}

bool AdbShell::connect(const QString &deviceId)
{
    if(deviceId.isEmpty())
        return false;
    if(isConnect())
    {
        if(this->deviceId() == deviceId)
            return true;
        exit();
    }
    if(Adb::deviceStatus(deviceId) != AdbConStatus::DEVICE)
        return false;
    return (ref = refGlobal(deviceId)) != nullptr && isConnect();
}

bool AdbShell::isConnect()
{
    return ref && !ref->devId.isEmpty() && ref->data.load() > 0;
}

std::pair<bool, QString> AdbShell::commandQueueWait(const QStringList &args)
{
    int reqId = commandQueueAsync(args);
    std::pair<bool, QString> result {false, {}};
    if(reqId != -1)
        result = commandResult(reqId, true);
    return result;
}

int AdbShell::commandQueueAsync(const QStringList &args)
{
    int reqId;
    if(args.empty() || !isConnect())
        return -1;
    std::lock_guard<std::mutex> lock(ref->mutex);
    auto inRequests = [&]()
    {
        for(const auto &r : ref->requests)
            if(r.first == reqId)
                return true;
        return false;
    };
    do
    {
        reqId = QRandomGenerator::global()->bounded(1, std::numeric_limits<int>::max());
    } while(inRequests() || ref->responces.find(reqId) != ref->responces.end());
    ref->requests.push_back({reqId, args});
    return reqId;
}

std::pair<bool, QString> AdbShell::commandResult(int requestId, bool waitResult)
{
    bool found = false;
    bool success = false;
    QString output {};

    if(!ref || !hasReqID(requestId))
        return {false, {}};

    const int maxWaitMs = ExecWaitTime + 2000;
    int elapsedMs = 0;

    do
    {
        {
            std::lock_guard<std::mutex> lock(ref->mutex);
            auto iter = ref->responces.find(requestId);
            if(iter != ref->responces.end())
            {
                found = true;
                success = (iter->second.exitCode == 0);
                output = std::move(iter->second.output);
                ref->responces.erase(iter);
                break;
            }
        }

        if(!waitResult)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        elapsedMs += 10;
        if(elapsedMs >= maxWaitMs)
        {
            qWarning() << "AdbShell::commandResult timeout waiting for requestId:" << requestId;
            break;
        }

    } while(isConnect() && !found);

    return {found && success, output};
}

bool AdbShell::hasReqID(int requestId)
{
    if(!ref)
        return false;
    std::lock_guard<std::mutex> lock(ref->mutex);
    for(const auto &r : ref->requests)
    {
        if(r.first == requestId)
            return true;
    }
    return (ref->responces.find(requestId) != ref->responces.end());
}

QString AdbShell::getprop(const QString &propname)
{
    return commandQueueWaits("getprop", propname).second.trimmed();
}

bool AdbShell::reConnect()
{
    if(ref == nullptr)
        return false;
    QString devId = ref->devId;
    exit();
    return connect(devId);
}

void AdbShell::exit()
{
    if(ref)
    {
        unrefGlobal(ref);
        ref.reset();
    }
}
