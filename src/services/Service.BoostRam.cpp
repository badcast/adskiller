#include <tuple>
#include <memory>
#include <functional>
#include <chrono>

#include "Services.h"
#include "mainwindow.h"

#define FORCLYQUIT_CHECK             \
    if(bCmd == CommandExecForceKill) \
    break
#define WAIT(MS) QThread::msleep(MS)
#define WAITMODE      \
    FORCLYQUIT_CHECK; \
    WAIT(400)
#define WAITMODE2     \
    FORCLYQUIT_CHECK; \
    WAIT(50)
#define PRINT_LINE boostram_write_log("--------------------------------------------------------")

enum
{
    CommandExecFailed = -1,
    CommandExecForceKill = -2
};

enum BoostRamStatus
{
    Idle = 0,
    Running,
    Error
};

static QStringList bOutLogs;
static QString bOutHeads;
static QString bAdbDeviceSerial;
static QThread *bBoostThread = nullptr;
static QMutex *bMutex = nullptr;
static BoostRamStatus bStatus = BoostRamStatus::Idle;
static int bProgress = 0;
static int bCmd = 0;

static void boostram_kill_proc();
static bool boostram_clean_cmd();
static void boostram_awake(BoostRamService *service);

static inline std::pair<QStringList, int> boostram_read_log()
{
    QMutexLocker locker(bMutex);
    (void) locker;
    return {QStringList(std::move(bOutLogs)), bProgress};
}

static inline QString boostram_read_head()
{
    QMutexLocker locker(bMutex);
    (void) locker;
    return bOutHeads;
}

static inline void boostram_write_log(QString msg, int progress = -1)
{
    QMutexLocker locker(bMutex);
    (void) locker;
    bOutLogs << std::move(msg.split('\n'));
    if(progress > -1)
        bProgress = progress;
}

static inline void boostram_write_log_head(QString msg, int progress = -1)
{
    QMutexLocker locker(bMutex);
    (void) locker;
    bOutHeads = std::move(msg);
    bOutLogs << bOutHeads;
    if(progress > -1)
        bProgress = progress;
}

BoostRamService::BoostRamService(QObject *parent) : Service(DeviceConnectType::ADB, parent), processLogStatus(nullptr), malwareStatusText0(nullptr), deviceLabelName(nullptr), processBarStatus(nullptr), pushButtonReRun(nullptr)
{
}

BoostRamService::~BoostRamService()
{
    stop();
}

QString BoostRamService::uuid() const
{
    return IDServiceBoostRamString;
}

QString BoostRamService::widgetIconName()
{
    return "white-boost-phone";
}

void BoostRamService::setArgs(const AdbDevice &adbDevice)
{
    Service::setArgs(adbDevice);
    MainWindow::current->accessUi_page_longinfo(processLogStatus, malwareStatusText0, deviceLabelName, processBarStatus, pushButtonReRun);
}

PageIndex BoostRamService::targetPage()
{
    return LongInfoPage;
}

bool BoostRamService::canStart()
{
    return Service::canStart() && processLogStatus && malwareStatusText0 && deviceLabelName && processBarStatus && pushButtonReRun;
}

bool BoostRamService::isStarted()
{
    return bStatus == BoostRamStatus::Running;
}

bool BoostRamService::isFinish()
{
    return bStatus != BoostRamStatus::Running;
}

static QString generateDeviceDashboardHtml(const AdbDevice &device, const std::shared_ptr<AdbSysInfo> &sysInfo)
{
    QString devTitle = !device.marketingName.isEmpty() ? device.marketingName : (!device.displayName.isEmpty() ? device.displayName : device.model);
    QString vendorStr = !device.vendor.isEmpty() ? device.vendor : "Android";
    QString osStr = sysInfo ? sysInfo->OSVersionString() : "Android";
    QString storageStr = sysInfo ? sysInfo->StorageDesignString() : "—";
    QString ramStr = sysInfo ? sysInfo->RAMDesignString() : "—";
    QString kernelStr = (sysInfo && !sysInfo->kernelReleaseVersion.isEmpty()) ? sysInfo->kernelReleaseVersion.trimmed() : "Linux";
    QString archStr = (sysInfo && !sysInfo->machine.isEmpty()) ? sysInfo->machine.trimmed() : "aarch64";
    QString serialStr = !device.devId.isEmpty() ? device.devId : "USB";

    return QString(
               "<div style='padding: 2px 4px;'>"
               "  <table width='100%' border='0' cellpadding='0' cellspacing='0' style='margin-bottom: 6px;'>"
               "    <tr>"
               "      <td align='left' style='font-size: 13px; font-weight: bold; color: #FFFFFF;'>"
               "        📱 %1 <span style='font-size: 11px; color: #8E9297;'>(%2)</span>"
               "      </td>"
               "      <td align='right' style='font-size: 11px; color: #00E676; font-weight: bold;'>"
               "        ● Подключено [ %3 ]"
               "      </td>"
               "    </tr>"
               "  </table>"
               "  <table width='100%' border='0' cellpadding='4' cellspacing='3' style='font-size: 10.5px; color: #BAC0CB; background: rgba(0,0,0,0.3); border-radius: 6px;'>"
               "    <tr>"
               "      <td>🤖 <b>ОС:</b> <span style='color: #4CC2FF; font-weight: bold;'>%4</span></td>"
               "      <td>💾 <b>Хранилище:</b> <span style='color: #00E5FF; font-weight: bold;'>%5</span></td>"
               "      <td>⚡ <b>ОЗУ:</b> <span style='color: #FFD700; font-weight: bold;'>%6</span></td>"
               "      <td>⚙️ <b>Архитектура:</b> <span style='color: #E3E5E8; font-weight: bold;'>%7 (%8)</span></td>"
               "    </tr>"
               "  </table>"
               "</div>")
        .arg(vendorStr + " " + devTitle, device.model, serialStr, osStr, storageStr, ramStr, archStr, kernelStr);
}

bool BoostRamService::start()
{
    if(!canStart())
        return false;
    if(pushButtonReRun)
        pushButtonReRun->setEnabled(false);

    QStringListModel *model = static_cast<QStringListModel *>(processLogStatus->model());
    QStringList place = model->stringList();
    QString deviceName = QString("Выбранное устройство (%1)").arg(mAdbDevice.displayName);

    circleRamStateReset();

    processBarStatus->setValue(0);
    MainWindow::current->malwareProgressCircle->setInfinilyMode(true);
    MainWindow::current->malwareProgressCircle->setValue(0);

    AdbDevice dev = mAdbDevice;
    if(dev.isEmpty() && !bAdbDeviceSerial.isEmpty())
        dev = Adb::getDevice(bAdbDeviceSerial);

    std::shared_ptr<AdbSysInfo> initialInfo = AdbShell(dev.devId).getInfo();
    deviceLabelName->setText(generateDeviceDashboardHtml(dev, initialInfo));

    place << "<< Запуск процедуры оптимизации ОЗУ (Boost RAM) >>";
    place << "<< Пожалуйста, не отсоединяйте устройство от компьютера >>";
    model->setStringList(place);

    MainWindow::current->delayUICall(
        500,
        [this]()
        {
            QTimer *updateTimer = new QTimer(this);
            updateTimer->start(100);
            QObject::connect(
                updateTimer,
                &QTimer::timeout,
                this,
                [this, updateTimer]()
                {
                    QStringListModel *model = static_cast<QStringListModel *>(processLogStatus->model());
                    QString header = boostram_read_head();
                    std::pair<QStringList, int> reads = boostram_read_log();

                    malwareStatusText0->setText(header);
                    processBarStatus->setValue(reads.second);
                    MainWindow::current->malwareProgressCircle->setValue(reads.second);
                    if(!reads.first.isEmpty())
                    {
                        QStringList from = model->stringList();
                        from.append(reads.first);
                        model->setStringList(from);
                        processLogStatus->scrollToBottom();
                    }
                    if(bStatus != BoostRamStatus::Running)
                    {
                        circleRamState(bStatus != BoostRamStatus::Error);
                        MainWindow::current->malwareProgressCircle->setInfinilyMode(false);
                        updateTimer->stop();
                        updateTimer->deleteLater();
                        if(pushButtonReRun)
                            pushButtonReRun->setEnabled(true);
                        boostram_clean_cmd();
                    }
                });
        });

    if(bBoostThread != nullptr)
    {
        boostram_write_log("Процесс уже запущен.");
        return false;
    }

    bMutex = new QMutex();
    bBoostThread = new QThread();
    QObject::connect(bBoostThread, &QThread::started, std::bind(boostram_awake, this));
    bCmd = 0;
    bProgress = 0;
    bAdbDeviceSerial = mAdbDevice.devId;
    bStatus = BoostRamStatus::Running;
    bBoostThread->start();

    return isStarted();
}

void BoostRamService::stop()
{
    if(isStarted())
        boostram_kill_proc();
    if(isFinish())
        circleRamStateReset();
    if(pushButtonReRun)
        pushButtonReRun->setEnabled(true);
}

void BoostRamService::circleRamState(bool success)
{
    MainWindow::current->malwareProgressCircle->setInfinilyMode(false);

    QPropertyAnimation *animation;
    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "outerRadius", MainWindow::current->malwareProgressCircle);
    animation->setDuration(1500);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(0.8);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "innerRadius", MainWindow::current->malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    QColor color = success ? QColor(80, 200, 120) : QColor(255, 100, 100);

    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "color", MainWindow::current->malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(color);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void BoostRamService::circleRamStateReset()
{
    QPropertyAnimation *animation;
    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "outerRadius", MainWindow::current->malwareProgressCircle);
    animation->setDuration(1500);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "innerRadius", MainWindow::current->malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(0.6);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    QColor color {110, 190, 235};

    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "color", MainWindow::current->malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(color);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

static bool boostram_clean_cmd()
{
    if(bStatus == BoostRamStatus::Running)
        return false;
    if(bMutex)
    {
        delete bMutex;
        bMutex = nullptr;
    }
    if(bBoostThread)
    {
        bBoostThread->quit();
        bBoostThread->wait();
        delete bBoostThread;
        bBoostThread = nullptr;
    }
    return true;
}

static void boostram_kill_proc()
{
    if(bBoostThread == nullptr)
        return;
    bCmd = CommandExecForceKill;
}

static void boostram_awake(BoostRamService *service)
{
    (void) service;
    using namespace std::chrono;

    int isFinish = 0;
    int stoppedCount = 0;
    std::shared_ptr<AdbSysInfo> sysInfoBefore, sysInfoAfter;
    auto procedureStartAt = steady_clock::now();

    AdbDevice device = Adb::getDevice(bAdbDeviceSerial);

    const std::function<QString(const std::shared_ptr<AdbSysInfo> &)> print_device_info = [&device](const std::shared_ptr<AdbSysInfo> &sysInfo) -> QString
    {
        if(!sysInfo)
            return {};
        return QString(
                   "------------------------------------\n"
                   "       Модель: %1\n"
                   "       Производитель: %2\n"
                   "       ОС: %3\n"
                   "       Хранилище: %4\n"
                   "       ОЗУ: %5\n"
                   "       Ядро: %6\n"
                   "       Архитектура: %7\n"
                   "------------------------------------")
            .arg(device.model, device.vendor, sysInfo->OSVersionString(), sysInfo->StorageDesignString(), sysInfo->RAMDesignString(), sysInfo->systemName, sysInfo->machine);
    };

    const std::function<bool(void)> check_device_connect = []() -> bool { return Adb::deviceStatus(bAdbDeviceSerial) == DEVICE; };

    while(!isFinish)
    {
        switch(bCmd)
        {
            // INIT
            case 0:
            {
                boostram_write_log_head("Запуск процедуры оптимизации ОЗУ (Boost RAM)...", 1);
                stoppedCount = 0;
                bCmd++;
                WAITMODE;
                break;
            }
            // READ SYSINFO & PACKAGES
            case 1:
            {
                if(!check_device_connect())
                {
                    boostram_write_log_head("Устройство отключено.");
                    boostram_write_log("Пожалуйста, убедитесь что устройство подключено корректно.");
                    bCmd = CommandExecFailed;
                    break;
                }

                boostram_write_log_head(QString("Получение данных с устройства ") + device.displayName + " (" + device.devId + ")...", 5);
                sysInfoBefore = AdbShell(device.devId).getInfo();
                if(sysInfoBefore)
                {
                    boostram_write_log(print_device_info(sysInfoBefore));
                    qint64 totalRamMb = sysInfoBefore->ramTotal / (1024 * 1024);
                    qint64 usedRamMb = sysInfoBefore->ramUsed / (1024 * 1024);
                    qint64 freeRamMb = qMax<qint64>(0, totalRamMb - usedRamMb);
                    int ramPercent = sysInfoBefore->ramTotal > 0 ? static_cast<int>((sysInfoBefore->ramUsed * 100) / sysInfoBefore->ramTotal) : 0;

                    boostram_write_log(QString("Текущая загрузка ОЗУ: %1% (Занято: %2 МБ / Всего: %3 МБ, Свободно: %4 МБ)").arg(ramPercent).arg(usedRamMb).arg(totalRamMb).arg(freeRamMb), 10);
                }
                WAITMODE;

                boostram_write_log_head("Сканирование активных и установленных приложений...", 15);
                QList<PackageIO> packages = Adb::getPackages(bAdbDeviceSerial);
                if(!check_device_connect())
                {
                    boostram_write_log_head("Устройство внезапно отключилось.");
                    bCmd = CommandExecFailed;
                    break;
                }

                boostram_write_log(QString("Найдено приложений для оптимизации: %1").arg(packages.size()), 20);
                WAITMODE;

                if(packages.isEmpty())
                {
                    boostram_write_log("Нет сторонних приложений для остановки.");
                }
                else
                {
                    boostram_write_log_head("Остановка фоновых приложений и освобождение памяти...", 25);
                    std::unique_ptr<AdbShell> shell = std::make_unique<AdbShell>(bAdbDeviceSerial);
                    int total = packages.size();
                    int currentIdx = 0;

                    for(const PackageIO &pkg : packages)
                    {
                        FORCLYQUIT_CHECK;
                        if(!check_device_connect())
                        {
                            boostram_write_log_head("Устройство отключилось во время остановки приложений.");
                            bCmd = CommandExecFailed;
                            break;
                        }

                        if(shell && shell->isConnect())
                        {
                            auto reply = shell->commandQueueWait(QStringList() << "am" << "force-stop" << pkg.packageName);
                            if(reply.first)
                            {
                                stoppedCount++;
                                boostram_write_log(QString(" [✓] Остановлено: %1").arg(pkg.packageName));
                            }
                            else
                            {
                                boostram_write_log(QString(" [-] Пропуск: %1").arg(pkg.packageName));
                            }
                        }

                        currentIdx++;
                        int progressVal = 25 + (currentIdx * 65) / total;
                        bProgress = qMin(90, progressVal);
                        WAITMODE2;
                    }

                    if(bCmd == CommandExecForceKill || bCmd == CommandExecFailed)
                        break;
                }

                boostram_write_log_head("Сброс системных буферов и финализация...", 92);
                {
                    AdbShell shell(bAdbDeviceSerial);
                    if(shell.isConnect())
                    {
                        shell.commandQueueWait(QStringList() << "am" << "kill-all");
                        shell.commandQueueWait(QStringList() << "sync");
                    }
                }
                WAITMODE;

                sysInfoAfter = AdbShell(device.devId).getInfo();
                if(sysInfoAfter)
                {
                    qint64 totalRamMb = sysInfoAfter->ramTotal / (1024 * 1024);
                    qint64 usedRamMb = sysInfoAfter->ramUsed / (1024 * 1024);
                    qint64 freeRamMb = qMax<qint64>(0, totalRamMb - usedRamMb);
                    int ramPercent = sysInfoAfter->ramTotal > 0 ? static_cast<int>((sysInfoAfter->ramUsed * 100) / sysInfoAfter->ramTotal) : 0;

                    PRINT_LINE;
                    boostram_write_log(QString("Итоговая загрузка ОЗУ: %1% (Занято: %2 МБ / Всего: %3 МБ, Свободно: %4 МБ)").arg(ramPercent).arg(usedRamMb).arg(totalRamMb).arg(freeRamMb));
                }

                bProgress = 100;
                bCmd++;
                break;
            }
            default:
            {
                PRINT_LINE;
                WAIT(1000);
                isFinish = 1;
                if(bCmd < 0)
                {
                    boostram_write_log_head("Процедура Boost RAM завершилась с ошибкой.", 100);
                    bStatus = BoostRamStatus::Error;
                }
                else
                {
                    auto durationProcedure = duration_cast<seconds>(steady_clock::now() - procedureStartAt);
                    QString text = QString("Оптимизация завершена! Остановлено приложений: %1. Затрачено: %2 с.").arg(stoppedCount).arg(durationProcedure.count());
                    boostram_write_log_head(text, 100);
                    bStatus = BoostRamStatus::Idle;
                }
                break;
            }
        }
    }
}
