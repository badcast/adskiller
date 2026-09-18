#include <algorithm>
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
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include "adbcmds.h"
#include "adbfront.h"
#include "AdbShellDexApp.h"

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

QString AdbShell::sdkToAndroidVersion(int sdk)
{
    switch(sdk)
    {
        case 35:
            return "Android 15";
        case 34:
            return "Android 14 (Upside Down Cake)";
        case 33:
            return "Android 13 (Tiramisu)";
        case 32:
            return "Android 12L";
        case 31:
            return "Android 12 (Snow Cone)";
        case 30:
            return "Android 11 (Red Velvet Cake)";
        case 29:
            return "Android 10 (Quince Tart)";
        case 28:
            return "Android 9.0 (Pie)";
        case 27:
            return "Android 8.1 (Oreo MR1)";
        case 26:
            return "Android 8.0 (Oreo)";
        case 25:
            return "Android 7.1 (Nougat MR1)";
        case 24:
            return "Android 7.0 (Nougat)";
        case 23:
            return "Android 6.0 (Marshmallow)";
        case 22:
            return "Android 5.1 (Lollipop MR1)";
        case 21:
            return "Android 5.0 (Lollipop)";
        case 19:
            return "Android 4.4 (KitKat)";
        default:
            if(sdk > 35)
                return QString("Android API %1").arg(sdk);
            if(sdk > 0)
                return QString("Android (API %1)").arg(sdk);
            return "Не указан";
    }
}

QString AdbShell::friendlyAppName(const QString &pkgName)
{
    static const QHash<QString, QString> knownApps = {
        {"com.whatsapp", "WhatsApp"},
        {"com.whatsapp.w4b", "WhatsApp Business"},
        {"org.telegram.messenger", "Telegram"},
        {"org.telegram.plus", "Telegram Plus"},
        {"com.instagram.android", "Instagram"},
        {"com.facebook.katana", "Facebook"},
        {"com.facebook.orca", "Messenger"},
        {"com.facebook.lite", "Facebook Lite"},
        {"com.google.android.youtube", "YouTube"},
        {"com.google.android.apps.youtube.music", "YouTube Music"},
        {"com.android.chrome", "Google Chrome"},
        {"com.google.android.gm", "Gmail"},
        {"com.google.android.apps.maps", "Google Карты"},
        {"com.google.android.apps.photos", "Google Фото"},
        {"com.google.android.googlequicksearchbox", "Google Поиск"},
        {"com.google.android.play.games", "Google Play Игры"},
        {"com.android.vending", "Google Play Маркет"},
        {"com.google.android.calendar", "Google Календарь"},
        {"com.google.android.contacts", "Google Контакты"},
        {"com.google.android.dialer", "Google Телефон"},
        {"com.google.android.apps.messaging", "Google Сообщения"},
        {"com.google.android.keep", "Google Заметки"},
        {"com.google.android.apps.docs", "Google Документы"},
        {"com.spotify.music", "Spotify"},
        {"com.zhiliaoapp.musically", "TikTok"},
        {"com.ss.android.ugc.trill", "TikTok"},
        {"com.vkontakte.android", "ВКонтакте"},
        {"ru.yandex.searchplugin", "Яндекс с Алисой"},
        {"com.yandex.browser", "Яндекс Браузер"},
        {"ru.yandex.taxi", "Яндекс Go"},
        {"ru.yandex.music", "Яндекс Музыка"},
        {"ru.yandex.yandexmaps", "Яндекс Карты"},
        {"ru.sberbankmobile", "СберБанк Онлайн"},
        {"ru.tinkoff.activities", "Т-Банк (Тинькофф)"},
        {"com.viber.voip", "Viber"},
        {"com.discord", "Discord"},
        {"com.opera.browser", "Opera"},
        {"com.opera.mini.native", "Opera Mini"},
        {"org.mozilla.firefox", "Firefox"},
        {"com.miui.gallery", "Галерея Xiaomi"},
        {"com.miui.cleanmaster", "Безопасность Xiaomi"},
        {"com.sec.android.app.camera", "Камера Samsung"},
        {"com.sec.android.gallery3d", "Галерея Samsung"},
        {"com.android.settings", "Настройки"},
        {"com.android.camera", "Камера"},
        {"com.android.gallery3d", "Галерея"},
        {"com.android.contacts", "Контакты"},
        {"com.android.mms", "Сообщения"},
        {"com.android.dialer", "Телефон"},
        {"com.android.providers.media", "Хранилище мультимедиа"},
        {"com.android.providers.downloads", "Загрузки"},
        {"com.android.packageinstaller", "Установщик пакетов"}};

    if(knownApps.contains(pkgName))
        return knownApps.value(pkgName);

    QStringList parts = pkgName.split('.', Qt::SkipEmptyParts);
    if(parts.isEmpty())
        return pkgName;

    QString cand = parts.last();
    if((cand == "android" || cand == "app" || cand == "mobile" || cand == "client") && parts.size() > 1)
        cand = parts[parts.size() - 2];

    if(!cand.isEmpty())
    {
        cand[0] = cand[0].toUpper();
        return cand;
    }

    return pkgName;
}

AdbPackageDetails AdbShell::parseDumpsys(const QString &dumpsysOutput, const QString &pkgName)
{
    AdbPackageDetails details;
    details.packageName = pkgName;
    details.rawDumpsys = dumpsysOutput;

    static QRegularExpression reVerName("versionName=([^\r\n\\s]+)");
    auto mVerName = reVerName.match(dumpsysOutput);
    if(mVerName.hasMatch())
        details.versionName = mVerName.captured(1);

    static QRegularExpression reVerCode("versionCode=(\\d+)");
    auto mVerCode = reVerCode.match(dumpsysOutput);
    if(mVerCode.hasMatch())
        details.versionCode = mVerCode.captured(1);

    static QRegularExpression reMinSdk("minSdk=(\\d+)");
    auto mMinSdk = reMinSdk.match(dumpsysOutput);
    if(mMinSdk.hasMatch())
        details.minSdk = mMinSdk.captured(1);

    static QRegularExpression reTargetSdk("targetSdk=(\\d+)");
    auto mTargetSdk = reTargetSdk.match(dumpsysOutput);
    if(mTargetSdk.hasMatch())
        details.targetSdk = mTargetSdk.captured(1);

    static QRegularExpression reCodePath("codePath=([^\r\n\\s]+)");
    auto mCodePath = reCodePath.match(dumpsysOutput);
    if(mCodePath.hasMatch())
        details.codePath = mCodePath.captured(1);

    static QRegularExpression reDataDir("dataDir=([^\r\n\\s]+)");
    auto mDataDir = reDataDir.match(dumpsysOutput);
    if(mDataDir.hasMatch())
        details.dataDir = mDataDir.captured(1);

    static QRegularExpression reFirstInstall("firstInstallTime=([0-9\\-]+\\s+[0-9:]+)");
    auto mFirstInstall = reFirstInstall.match(dumpsysOutput);
    if(mFirstInstall.hasMatch())
        details.firstInstallTime = mFirstInstall.captured(1);

    static QRegularExpression reLastUpdate("lastUpdateTime=([0-9\\-]+\\s+[0-9:]+)");
    auto mLastUpdate = reLastUpdate.match(dumpsysOutput);
    if(mLastUpdate.hasMatch())
        details.lastUpdateTime = mLastUpdate.captured(1);

    static QRegularExpression reInstaller("installerPackageName=([^\r\n\\s]+)");
    auto mInstaller = reInstaller.match(dumpsysOutput);
    if(mInstaller.hasMatch())
        details.installer = mInstaller.captured(1);

    static QRegularExpression reAbi("primaryCpuAbi=([^\r\n\\s]+)");
    auto mAbi = reAbi.match(dumpsysOutput);
    if(mAbi.hasMatch())
        details.primaryCpuAbi = mAbi.captured(1);

    static QRegularExpression reSignatures("signatures=PackageSignatures\\{([^\\}]+)\\}");
    auto mSignatures = reSignatures.match(dumpsysOutput);
    if(mSignatures.hasMatch())
        details.signatures = mSignatures.captured(1);

    int reqIdx = dumpsysOutput.indexOf("requested permissions:");
    if(reqIdx != -1)
    {
        int endIdx = dumpsysOutput.indexOf("\n    install permissions:", reqIdx);
        if(endIdx == -1)
            endIdx = dumpsysOutput.indexOf("\n    runtime permissions:", reqIdx);
        if(endIdx == -1)
            endIdx = dumpsysOutput.indexOf("\n  User 0:", reqIdx);

        QString permBlock = (endIdx != -1) ? dumpsysOutput.mid(reqIdx, endIdx - reqIdx) : dumpsysOutput.mid(reqIdx, 4000);
        QStringList lines = permBlock.split('\n');
        for(const QString &l : lines)
        {
            QString trimmed = l.trimmed();
            if(trimmed.startsWith("android.permission.") || trimmed.contains(".permission."))
            {
                if(!details.requestedPermissions.contains(trimmed))
                    details.requestedPermissions.append(trimmed);
            }
        }
    }

    static QRegularExpression reGranted("([a-zA-Z0-9_\\.]+):\\s*granted=true");
    auto matchIter = reGranted.globalMatch(dumpsysOutput);
    while(matchIter.hasNext())
    {
        auto match = matchIter.next();
        details.grantedPermissions.insert(match.captured(1));
    }

    int actIdx = dumpsysOutput.indexOf("Activity Resolver Table:");
    if(actIdx != -1)
    {
        int actEnd = dumpsysOutput.indexOf("Receiver Resolver Table:", actIdx);
        if(actEnd == -1)
            actEnd = dumpsysOutput.indexOf("Service Resolver Table:", actIdx);
        QString actBlock = (actEnd != -1) ? dumpsysOutput.mid(actIdx, actEnd - actIdx) : dumpsysOutput.mid(actIdx, 3000);
        static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
        auto actMatch = reComp.globalMatch(actBlock);
        while(actMatch.hasNext())
        {
            QString act = actMatch.next().captured(1);
            if(!details.activities.contains(act))
                details.activities.append(act);
        }
    }
    if(!details.activities.isEmpty())
        details.mainActivity = details.activities.first();

    int srvIdx = dumpsysOutput.indexOf("Service Resolver Table:");
    if(srvIdx != -1)
    {
        int srvEnd = dumpsysOutput.indexOf("Provider Resolver Table:", srvIdx);
        QString srvBlock = (srvEnd != -1) ? dumpsysOutput.mid(srvIdx, srvEnd - srvIdx) : dumpsysOutput.mid(srvIdx, 3000);
        static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
        auto srvMatch = reComp.globalMatch(srvBlock);
        while(srvMatch.hasNext())
        {
            QString srv = srvMatch.next().captured(1);
            if(!details.services.contains(srv))
                details.services.append(srv);
        }
    }

    int recIdx = dumpsysOutput.indexOf("Receiver Resolver Table:");
    if(recIdx != -1)
    {
        int recEnd = dumpsysOutput.indexOf("Service Resolver Table:", recIdx);
        QString recBlock = (recEnd != -1) ? dumpsysOutput.mid(recIdx, recEnd - recIdx) : dumpsysOutput.mid(recIdx, 3000);
        static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
        auto recMatch = reComp.globalMatch(recBlock);
        while(recMatch.hasNext())
        {
            QString rec = recMatch.next().captured(1);
            if(!details.receivers.contains(rec))
                details.receivers.append(rec);
        }
    }

    int prvIdx = dumpsysOutput.indexOf("Provider Resolver Table:");
    if(prvIdx != -1)
    {
        int prvEnd = dumpsysOutput.indexOf("\n\n", prvIdx);
        QString prvBlock = (prvEnd != -1) ? dumpsysOutput.mid(prvIdx, prvEnd - prvIdx) : dumpsysOutput.mid(prvIdx, 3000);
        static QRegularExpression reComp(pkgName + "/([a-zA-Z0-9_\\.]+)");
        auto prvMatch = reComp.globalMatch(prvBlock);
        while(prvMatch.hasNext())
        {
            QString prv = prvMatch.next().captured(1);
            if(!details.providers.contains(prv))
                details.providers.append(prv);
        }
    }

    return details;
}

QList<AdbPackageInfo> AdbShell::getPackageList()
{
    if(!isConnect())
        return {};

    // 1. Try modern app_process DexAgent (100% accurate localized names, 10x faster)
    AdbShellDexApp dexApp(*this);
    auto dexList = dexApp.getPackageList();
    if(!dexList.isEmpty())
        return dexList;

    // 2. Fallback to legacy pm list packages
    QList<AdbPackageInfo> result;

    auto replyPkgs = commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-f" << "-u");
    auto replyUser = commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-3");
    auto replyDis = commandQueueWait(QStringList() << "pm" << "list" << "packages" << "-d");

    if(!replyPkgs.first)
        return result;

    QSet<QString> userPkgs;
    if(replyUser.first)
    {
        QStringList uLines = replyUser.second.split('\n', Qt::SkipEmptyParts);
        for(QString &ul : uLines)
        {
            ul = ul.trimmed();
            if(ul.startsWith("package:"))
                ul.remove(0, 8);
            userPkgs.insert(ul.trimmed());
        }
    }

    QSet<QString> disabledPkgs;
    if(replyDis.first)
    {
        QStringList dLines = replyDis.second.split('\n', Qt::SkipEmptyParts);
        for(QString &dl : dLines)
        {
            dl = dl.trimmed();
            if(dl.startsWith("package:"))
                dl.remove(0, 8);
            disabledPkgs.insert(dl.trimmed());
        }
    }

    QStringList lines = replyPkgs.second.split('\n', Qt::SkipEmptyParts);
    for(const QString &line : lines)
    {
        QString clean = line.trimmed();
        if(clean.startsWith("package:"))
            clean.remove(0, 8);

        int eqIdx = clean.lastIndexOf('=');
        if(eqIdx == -1)
            continue;

        QString apkPath = clean.left(eqIdx).trimmed();
        QString pkgName = clean.mid(eqIdx + 1).trimmed();
        if(pkgName.isEmpty())
            continue;

        AdbPackageInfo info;
        info.packageName = pkgName;
        info.apkPath = apkPath;
        info.isSystem = !userPkgs.contains(pkgName);
        info.isDisabled = disabledPkgs.contains(pkgName);
        info.appName = friendlyAppName(pkgName);

        result.append(std::move(info));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const AdbPackageInfo &a, const AdbPackageInfo &b)
        {
            if(a.isSystem != b.isSystem)
                return !a.isSystem;
            return a.appName.localeAwareCompare(b.appName) < 0;
        });

    return result;
}

AdbPackageDetails AdbShell::getPackageDetails(const QString &packageName)
{
    AdbPackageDetails details;
    if(!isConnect() || packageName.isEmpty())
        return details;

    // 1. Try DexAgent
    AdbShellDexApp dexApp(*this);
    details = dexApp.getPackageDetails(packageName);
    if(!details.packageName.isEmpty())
        return details;

    // 2. Fallback to dumpsys
    auto dumpsysReply = commandQueueWait(QStringList() << "dumpsys" << "package" << packageName);
    QString dumpsys = dumpsysReply.first ? dumpsysReply.second : "";
    details = parseDumpsys(dumpsys, packageName);
    details.appName = friendlyAppName(packageName);

    if(!details.codePath.isEmpty() && details.codePath != "—")
    {
        auto sizeReply = commandQueueWait(QStringList() << "stat" << "-c" << "%s" << "\"" + details.codePath + "\"" << "2>/dev/null");
        if(sizeReply.first && !sizeReply.second.trimmed().isEmpty())
        {
            qint64 sz = sizeReply.second.trimmed().toLongLong();
            if(sz > 0)
                details.apkSize = sz;
        }
    }

    return details;
}

QByteArray AdbShell::extractPackageIconData(const QString &apkPath, const QString &packageName)
{
    if(!isConnect())
        return {};

    // 1. Try DexAgent for pixel-perfect system rendered icons (AdaptiveIconDrawable, VectorDrawable, PNG)
    if(!packageName.isEmpty())
    {
        AdbShellDexApp dexApp(*this);
        QByteArray dexIcon = dexApp.getAppIcon(packageName, 96);
        if(!dexIcon.isEmpty())
            return dexIcon;
    }

    if(apkPath.isEmpty())
        return {};

    auto listReply = commandQueueWait(
        QStringList() << "unzip" << "-l" << "\"" + apkPath + "\"" << "res/*ic_launcher*.png" << "res/*launcher*.png"
                      << "res/*icon*.png" << "res/*logo*.png"
                      << "*.webp" << "2>/dev/null");

    if(!listReply.first || listReply.second.isEmpty())
        return {};

    QString bestEntry;
    int bestScore = -1;

    QStringList lines = listReply.second.split('\n', Qt::SkipEmptyParts);
    for(const QString &line : lines)
    {
        QString trimmed = line.trimmed();
        int resIdx = trimmed.indexOf("res/");
        if(resIdx == -1)
            continue;

        QString entry = trimmed.mid(resIdx).trimmed();
        if(!entry.endsWith(".png", Qt::CaseInsensitive) && !entry.endsWith(".webp", Qt::CaseInsensitive))
            continue;

        int score = 0;
        if(entry.contains("xxxhdpi"))
            score += 50;
        else if(entry.contains("xxhdpi"))
            score += 40;
        else if(entry.contains("xhdpi"))
            score += 30;
        else if(entry.contains("hdpi"))
            score += 20;
        else if(entry.contains("mdpi"))
            score += 10;

        if(entry.contains("ic_launcher"))
            score += 15;
        else if(entry.contains("icon"))
            score += 10;

        if(score > bestScore)
        {
            bestScore = score;
            bestEntry = entry;
        }
    }

    if(bestEntry.isEmpty())
        return {};

    auto dumpReply = commandQueueWait(QStringList() << "unzip" << "-p" << "\"" + apkPath + "\"" << "\"" + bestEntry + "\"" << "2>/dev/null" << "|" << "base64");

    if(dumpReply.first && !dumpReply.second.isEmpty())
    {
        return QByteArray::fromBase64(dumpReply.second.toLatin1());
    }

    return {};
}

bool AdbShell::launchPackage(const QString &packageName)
{
    if(!isConnect() || packageName.isEmpty())
        return false;
    auto reply = commandQueueWait(QStringList() << "monkey" << "-p" << packageName << "-c" << "android.intent.category.LAUNCHER" << "1");
    return reply.first;
}

bool AdbShell::stopPackage(const QString &packageName)
{
    if(!isConnect() || packageName.isEmpty())
        return false;
    auto reply = commandQueueWait(QStringList() << "am" << "force-stop" << packageName);
    return reply.first;
}

bool AdbShell::setPackageEnabled(const QString &packageName, bool enabled)
{
    if(!isConnect() || packageName.isEmpty())
        return false;
    QString cmd = enabled ? "enable" : "disable-user";
    auto reply = commandQueueWait(QStringList() << "pm" << cmd << "--user" << "0" << packageName);
    return reply.first;
}

bool AdbShell::enablePackage(const QString &packageName)
{
    return setPackageEnabled(packageName, true);
}

bool AdbShell::disablePackage(const QString &packageName)
{
    return setPackageEnabled(packageName, false);
}

bool AdbShell::clearPackageData(const QString &packageName)
{
    if(!isConnect() || packageName.isEmpty())
        return false;
    auto reply = commandQueueWait(QStringList() << "pm" << "clear" << packageName);
    return reply.first;
}

bool AdbShell::uninstallPackage(const QString &packageName, bool keepData)
{
    if(!isConnect() || packageName.isEmpty())
        return false;
    QStringList args;
    args << "pm" << "uninstall";
    if(keepData)
        args << "-k";
    args << "--user" << "0" << packageName;
    auto reply = commandQueueWait(args);
    return reply.first && reply.second.contains("Success");
}

std::pair<bool, QString> AdbShell::installPackage(const QString &localApkPath, bool reinstall)
{
    QString dev = deviceId();
    if(dev.isEmpty() || localApkPath.isEmpty())
        return {false, "Device not connected or empty APK path"};
    int exitCode = -1;
    QStringList args = QStringList() << "-s" << dev << "install";
    if(reinstall)
        args << "-r";
    args << localApkPath;
    auto reply = adb_send_cmd(exitCode, args);
    bool ok = reply.first && (exitCode == 0) && (reply.second.contains("Success") || !reply.second.contains("Failure"));
    return {ok, reply.second};
}

bool AdbShell::pullFile(const QString &remotePath, const QString &localPath)
{
    QString dev = deviceId();
    if(dev.isEmpty() || remotePath.isEmpty() || localPath.isEmpty())
        return false;
    int exitCode = -1;
    adb_send_cmd(exitCode, QStringList() << "-s" << dev << "pull" << remotePath << localPath);
    return exitCode == 0;
}

bool AdbShell::pushFile(const QString &localPath, const QString &remotePath)
{
    QString dev = deviceId();
    if(dev.isEmpty() || localPath.isEmpty() || remotePath.isEmpty() || !QFile::exists(localPath))
        return false;
    int exitCode = -1;
    adb_send_cmd(exitCode, QStringList() << "-s" << dev << "push" << localPath << remotePath);
    return exitCode == 0;
}

