#include "Applefront.h"

#include <chrono>
#include <future>
#include <QDebug>
#include <QRandomGenerator>

struct AppleCmdResult
{
    int exitCode = -1;
    QString output;
};

struct AppleGlobal
{
    std::deque<std::tuple<int, QString, QStringList>> requests;
    std::unordered_map<int, AppleCmdResult> responses;
    std::thread *thread = nullptr;
    std::mutex mutex;
    QString devId;
    std::atomic<int> data {0};
    int ref {0};
};

static std::mutex s_appleGlobalsMutex;
static QHash<QString, std::shared_ptr<AppleGlobal>> s_appleGlobals;

static void AppleWorkerThread(AppleGlobal *global)
{
    if(!global) return;
    global->data.store(1);

    while(!global->devId.isEmpty() && global->data.load() > 0)
    {
        int reqId = -1;
        QString prog;
        QStringList args;

        {
            std::lock_guard<std::mutex> lock(global->mutex);
            if(!global->requests.empty())
            {
                auto &front = global->requests.front();
                reqId = std::get<0>(front);
                prog = std::get<1>(front);
                args = std::get<2>(front);
                global->requests.pop_front();
            }
        }

        if(reqId != -1)
        {
            int code = -1;
            auto reply = apple_send_cmd(code, prog, args);
            std::lock_guard<std::mutex> lock(global->mutex);
            global->responses[reqId] = AppleCmdResult{code, reply.second};
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
}

AppleShell::AppleShell(const QString &deviceId) : ref(nullptr)
{
    if(!deviceId.isEmpty())
        connect(deviceId);
}

AppleShell::AppleShell(AppleShell &&other) noexcept : ref(std::move(other.ref))
{
}

AppleShell &AppleShell::operator=(AppleShell &&other) noexcept
{
    if(this != &other)
    {
        exit();
        ref = std::move(other.ref);
    }
    return *this;
}

AppleShell::~AppleShell()
{
    exit();
}

bool AppleShell::connect(const QString &deviceId)
{
    QString targetDev = deviceId;
    if(targetDev.isEmpty())
    {
        QList<AppleDevice> devs = Apple::getDevices();
        if(!devs.isEmpty())
            targetDev = devs.first().devId;
        else
            return false;
    }

    std::lock_guard<std::mutex> lock(s_appleGlobalsMutex);
    if(s_appleGlobals.contains(targetDev))
    {
        ref = s_appleGlobals.value(targetDev);
        ref->ref++;
        return true;
    }

    ref = std::make_shared<AppleGlobal>();
    ref->devId = targetDev;
    ref->ref = 1;
    ref->thread = new std::thread(AppleWorkerThread, ref.get());
    s_appleGlobals.insert(targetDev, ref);
    return true;
}

bool AppleShell::isConnect() const
{
    return ref != nullptr && !ref->devId.isEmpty() && ref->data.load() > 0;
}

void AppleShell::exit()
{
    if(!ref) return;

    std::lock_guard<std::mutex> lock(s_appleGlobalsMutex);
    ref->ref--;
    if(ref->ref <= 0)
    {
        ref->data.store(0);
        if(ref->thread && ref->thread->joinable())
        {
            ref->thread->join();
            delete ref->thread;
            ref->thread = nullptr;
        }
        s_appleGlobals.remove(ref->devId);
    }
    ref.reset();
}

bool AppleShell::reConnect()
{
    if(!ref) return false;
    QString curId = ref->devId;
    exit();
    return connect(curId);
}

QString AppleShell::deviceId() const
{
    return ref ? ref->devId : QString();
}

std::pair<bool, QString> AppleShell::commandQueueWait(const QString &program, const QStringList &args)
{
    int exitCode = -1;
    return apple_send_cmd(exitCode, program, args);
}

int AppleShell::commandQueueAsync(const QString &program, const QStringList &args)
{
    if(!ref) return -1;
    int reqId = QRandomGenerator::global()->bounded(1, 100000000);
    std::lock_guard<std::mutex> lock(ref->mutex);
    ref->requests.push_back({reqId, program, args});
    return reqId;
}

std::pair<bool, QString> AppleShell::commandResult(int requestId, bool waitResult)
{
    if(!ref) return {false, {}};

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
    while(true)
    {
        {
            std::lock_guard<std::mutex> lock(ref->mutex);
            auto it = ref->responses.find(requestId);
            if(it != ref->responses.end())
            {
                bool success = (it->second.exitCode == 0);
                QString out = it->second.output;
                ref->responses.erase(it);
                return {success, out};
            }
        }

        if(!waitResult || std::chrono::steady_clock::now() >= deadline)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return {false, {}};
}

QString AppleShell::getprop(const QString &propname, const QString &domain)
{
    QString dev = deviceId();
    if(dev.isEmpty()) return {};

    QStringList args;
    if(!dev.startsWith("Apple-"))
        args << "-u" << dev;
    if(!domain.isEmpty())
        args << "-q" << domain;
    args << "-k" << propname;

    int exitCode = -1;
    auto reply = apple_send_cmd(exitCode, "ideviceinfo", args);
    return exitCode == 0 ? reply.second.trimmed() : QString();
}

std::shared_ptr<AppleSysInfo> AppleShell::getInfo()
{
    auto info = std::make_shared<AppleSysInfo>();
    info->systemName = "iOS";
    info->productType = getprop("ProductType");
    info->marketingName = Apple::marketingNameForModel(info->productType);
    info->productVersion = getprop("ProductVersion");
    info->buildVersion = getprop("BuildVersion");
    info->deviceName = getprop("DeviceName");
    info->serialNumber = getprop("SerialNumber");
    info->uniqueChipID = getprop("UniqueChipID");
    info->cpuArchitecture = getprop("CPUArchitecture");
    info->wifiAddress = getprop("WiFiAddress");
    info->bluetoothAddress = getprop("BluetoothAddress");
    info->activationState = getprop("ActivationState");
    info->passwordProtected = (getprop("PasswordProtected") == "true");

    QString totalDisk = getprop("TotalDiskCapacity", "com.apple.disk_usage");
    if(!totalDisk.isEmpty())
        info->diskTotal = totalDisk.toLongLong();
    QString availDisk = getprop("TotalDataAvailable", "com.apple.disk_usage");
    if(!availDisk.isEmpty() && info->diskTotal > 0)
        info->diskUsed = info->diskTotal - availDisk.toLongLong();

    QString battLevel = getprop("BatteryCurrentCapacity", "com.apple.mobile.battery");
    if(!battLevel.isEmpty())
        info->batteryLevel = battLevel.toInt();
    QString battCharging = getprop("BatteryIsCharging", "com.apple.mobile.battery");
    info->isCharging = (battCharging == "true");

    return info;
}

bool AppleShell::validatePairing()
{
    QString dev = deviceId();
    if(dev.isEmpty()) return false;
    int code = -1;
    auto reply = apple_send_cmd(code, "idevicepair", QStringList() << "-u" << dev << "validate");
    return code == 0 && reply.second.contains("SUCCESS");
}

bool AppleShell::requestPairing()
{
    QString dev = deviceId();
    if(dev.isEmpty()) return false;
    int code = -1;
    auto reply = apple_send_cmd(code, "idevicepair", QStringList() << "-u" << dev << "pair");
    return code == 0;
}

bool AppleShell::enterRecovery()
{
    QString dev = deviceId();
    if(dev.isEmpty()) return false;
    int code = -1;
    auto reply = apple_send_cmd(code, "ideviceenterrecovery", QStringList() << dev);
    return code == 0;
}

bool AppleShell::exitRecovery()
{
    int code = -1;
    auto reply = apple_send_cmd(code, "irecovery", QStringList() << "-n");
    return code == 0;
}

bool AppleShell::restartDevice()
{
    QString dev = deviceId();
    int code = -1;
    if(!dev.isEmpty() && !dev.startsWith("Apple-Recovery") && !dev.startsWith("Apple-DFU"))
    {
        auto reply = apple_send_cmd(code, "idevicediagnostics", QStringList() << "-u" << dev << "restart");
        if(code == 0) return true;
    }
    auto recReply = apple_send_cmd(code, "irecovery", QStringList() << "-c" << "reboot");
    return code == 0;
}

bool AppleShell::shutdownDevice()
{
    QString dev = deviceId();
    if(dev.isEmpty()) return false;
    int code = -1;
    auto reply = apple_send_cmd(code, "idevicediagnostics", QStringList() << "-u" << dev << "shutdown");
    return code == 0;
}
