#include "Services.h"
#include "mainwindow.h"

#define FORCLYQUIT_CHECK          \
    if(mCmd == MalwareForclyKill) \
    break
#define WAIT(MS) QThread::msleep(MS)
#define WAITMODE      \
    FORCLYQUIT_CHECK; \
    WAIT(400)
#define WAITMODE2     \
    FORCLYQUIT_CHECK; \
    WAIT(50)
#define PRINT_LINE malwareWriteLog("--------------------------------------------------------")

enum
{
    MalwareExecFail = -1,
    MalwareForclyKill = -2
};

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

QStringList outLogs;
QString outHeads;
QString adbDeviceSerial;
QThread *malwareThread;
QMutex *mutex;
MalwareStatus status;
Network *network;
int mProgress;
int mCmd;
int mUserValue;

void malwareStart(const QString &deviceSerial);
void malwareKill();
bool malwareClean();
void malwaring();
void malwareWriteVal(int userValue);

inline void malwareWriteVal(int userValue)
{
    QMutexLocker locker(mutex);
    (void) locker;
    mUserValue = userValue;
}
inline std::pair<QStringList, int> malwareReadLog()
{
    QMutexLocker locker(mutex);
    (void) locker;
    return {QStringList(std::move(outLogs)), mProgress};
}

inline QString malwareReadHeader()
{
    QMutexLocker locker(mutex);
    (void) locker;
    return outHeads;
}

void malwareWriteLog(QString msg, int progress = -1)
{
    QMutexLocker locker(mutex);
    (void) locker;
    outLogs << std::move(msg.split('\n'));
    if(progress > -1)
        mProgress = progress;
}

void malwareWriteHeader(QString msg, int progress = -1)
{
    QMutexLocker locker(mutex);
    (void) locker;
    outHeads = std::move(msg);
    outLogs << outHeads;
    if(progress > -1)
        mProgress = progress;
}

AdsKillerService::AdsKillerService(QObject *parent) : Service(DeviceConnectType::ADB, parent), processLogStatus(nullptr), malwareStatusText0(nullptr), deviceLabelName(nullptr), processBarStatus(nullptr), pushButtonReRun(nullptr)
{
}

void AdsKillerService::setArgs(const AdbDevice &adbDevice)
{
    Service::setArgs(adbDevice);
    MainWindow::current->accessUi_adskiller(processLogStatus, malwareStatusText0, deviceLabelName, processBarStatus, pushButtonReRun);
}

QString AdsKillerService::uuid() const
{
    return IDServiceAdsString;
}

PageIndex AdsKillerService::targetPage()
{
    return LongInfoPage;
}

bool AdsKillerService::canStart()
{
    return Service::canStart() && processLogStatus && malwareStatusText0 && deviceLabelName && processBarStatus && pushButtonReRun;
}

bool AdsKillerService::isStarted()
{
    return status == MalwareStatus::Running;
}

bool AdsKillerService::isFinish()
{
    return status != MalwareStatus::Running;
}

void AdsKillerService::reset()
{
    if(isFinish())
        cirlceMalwareStateReset();
    if(pushButtonReRun)
        pushButtonReRun->setEnabled(true);
}

QString AdsKillerService::widgetIconName()
{
    return "white-ads-remove";
}

bool AdsKillerService::start()
{
    if(!canStart())
        return false;
    pushButtonReRun->setEnabled(false);

    QStringListModel *model = static_cast<QStringListModel *>(processLogStatus->model());
    QStringList place = model->stringList();
    QString deviceName = "Выбранное устройство (%1)";
    deviceName = deviceName.arg(mAdbDevice.displayName);

    cirlceMalwareStateReset();

    processBarStatus->setValue(0);
    MainWindow::current->malwareProgressCircle->setInfinilyMode(true);
    MainWindow::current->malwareProgressCircle->setValue(0);

    deviceLabelName->setText(deviceName);

    place << "<< Запуск процесса удаления рекламы, пожалуйста подождите >>";
    place << "<< Не отсоединяйте устройство от компьютера >>";
    model->setStringList(place);

    MainWindow::current->delayPush(
        500,
        [this, deviceName]()
        {
            // START MALWARE
            QTimer *malwareUpdateTimer = new QTimer(this);
            malwareUpdateTimer->start(100);
            QObject::connect(
                malwareUpdateTimer,
                &QTimer::timeout,
                this,
                [this, deviceName, malwareUpdateTimer]()
                {
                    QStringListModel *model = static_cast<QStringListModel *>(processLogStatus->model());
                    QString header;
                    QStringList from;
                    std::pair<QStringList, int> reads;
                    header = malwareReadHeader();
                    reads = malwareReadLog();

                    malwareStatusText0->setText(header);
                    processBarStatus->setValue(reads.second);
                    MainWindow::current->malwareProgressCircle->setValue(reads.second);
                    if(!reads.first.isEmpty())
                    {
                        from = model->stringList();
                        from.append(reads.first);
                        model->setStringList(from);
                        processLogStatus->scrollToBottom();
                    }
                    if(status != MalwareStatus::Running)
                    {
                        cirlceMalwareState(status != MalwareStatus::Error);
                        MainWindow::current->malwareProgressCircle->setInfinilyMode(false);
                        malwareUpdateTimer->stop();
                        malwareUpdateTimer->deleteLater();
                        pushButtonReRun->setEnabled(true);
                        malwareClean();
                    }
                    else if(mUserValue == 1000)
                    {
                        UserDataInfo data = MainWindow::current->network.authedId;
                        QString buyText = "<!>\nПодтвердите свою покупку удаление вредоносных программ "
                                          "из устройства %1 за %2 %3\nВаш баланс %4 %5\nПосле покупки "
                                          "станет %6 %7\nЖелаете продолжить?\n<!>";
                        int num0 = qMax<int>(0, static_cast<int>(data.credits) - static_cast<int>(data.basePrice));
                        buyText = buyText.arg(deviceName).arg(data.basePrice).arg(data.currencyType).arg(data.credits).arg(data.currencyType).arg(num0).arg(data.currencyType);
                        num0 = QMessageBox::question(MainWindow::current, QString("Подтверждение покупки"), buyText, QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
                        malwareWriteVal(num0);
                    }
                });
        });

    malwareStart(mAdbDevice.devId);
    return isStarted();
}

void AdsKillerService::stop()
{
    if(isStarted())
        malwareKill();
}

void AdsKillerService::cirlceMalwareState(bool success)
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

    QColor color = success ? QColor(155, 219, 58) : QColor(255, 100, 100);

    animation = new QPropertyAnimation(MainWindow::current->malwareProgressCircle, "color", MainWindow::current->malwareProgressCircle);
    animation->setDuration(750);
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->setEndValue(color);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AdsKillerService::cirlceMalwareStateReset()
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

template <typename InT, typename OutT>
constexpr inline OutT map(const InT x, const InT in_min, const InT in_max, const OutT out_min, const OutT out_max)
{
    if(in_max == in_min)
    {
        return out_min;
    }
    OutT mapped_value = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    return qMin(qMax(mapped_value, out_min), out_max);
}

template <typename T0, typename T1, typename Pred>
inline T1 compare_list(const T0 &t0, const T1 &t1, Pred &&pred)
{
    T1 result;
    int x, y;
    for(x = 0; x < t0.size(); ++x)
    {
        for(y = 0; y < t1.size(); ++y)
        {
            if(pred(t0[x], t1[y]))
            {
                result << t1[y];
                break;
            }
        }
    }
    return result;
}

void malwareStart(const QString &deviceSerial)
{
    if(malwareThread != nullptr)
    {
        malwareWriteLog("Процесс уже запущен.");
        return;
    }
    adbDeviceSerial = deviceSerial;
    mutex = new QMutex();
    malwareThread = new QThread();
    QObject::connect(malwareThread, &QThread::started, &malwaring);
    network = new Network(MainWindow::current);
    network->authedId = MainWindow::current->network.authedId;
    mCmd = 0;
    mProgress = 0;
    status = MalwareStatus::Running;
    malwareThread->start();
}

bool malwareClean()
{
    if(status == MalwareStatus::Running)
        return false;
    delete network;
    delete mutex;
    malwareThread = nullptr;
    return true;
}

void malwareKill()
{
    if(malwareThread == nullptr)
        return;
    mCmd = MalwareForclyKill;
}

void malwaring()
{
    using namespace std::chrono;

    int isFinish = 0;
    int lastResult, num0, num1, totalMalwareDetected;
    QList<PackageIO> localPackages;
    QEventLoop loop;
    std::shared_ptr<AdbSysInfo> sysInfo;

    auto procedureStartAt = steady_clock::now();

    AdbDevice device = Adb::getDevice(adbDeviceSerial);

    const std::function<QString(void)> print_device_info = [&sysInfo, &device]() -> QString
    {
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
            .arg(device.model)
            .arg(device.vendor)
            .arg(sysInfo->OSVersionString())
            .arg(sysInfo->StorageDesignString())
            .arg(sysInfo->RAMDesignString())
            .arg(sysInfo->systemName)
            .arg(sysInfo->machine);
    };

    const std::function<QString(int)> generate_error_report = [](int status) -> QString
    {
        QString error;
        if(status == NetworkStatus::ServerError)
            error = ("Ошибка на стороне сервера.");
        else if(status == NetworkStatus::NoEnoughMoney)
            error = ("Пополните баланс, чтобы продолжить.");
        else
            error = (QString("Код ошибки %1").arg(status));
        return error;
    };

    const std::function<bool(void)> check_device_connect = []() -> bool { return Adb::deviceStatus(adbDeviceSerial) == DEVICE; };

    while(!isFinish)
    {
        switch(mCmd)
        {
                // INIT
            case 0:
            {
                malwareWriteHeader("Запуск процедуры удаление рекламы (Malware)...", 1);
                mUserValue = -1;
                totalMalwareDetected = 0;
                mCmd++;
                WAITMODE;
                break;
            }
            // GET PACKAGES & UPLOAD
            case 1:
            {
                if(!check_device_connect())
                {
                    malwareWriteHeader("Устройство внезапно отключилась.");
                    malwareWriteLog(
                        "Пожалуйста, убедитесь что устройство подключено "
                        "корректно и кабель не поврежден.");
                    mCmd = MalwareExecFail;
                    break;
                }
                malwareWriteHeader(QString("Получение данных с устройства ") + device.displayName + "(" + device.devId + ")", 2);
                sysInfo = AdbShell(device.devId).getInfo();
                malwareWriteLog(print_device_info());
                WAITMODE;

                localPackages = Adb::getPackages(adbDeviceSerial);
                if(check_device_connect())
                {
                    if(localPackages.isEmpty())
                    {
                        malwareWriteHeader("Получили пустой результат.");
                        mCmd = MalwareExecFail;
                        break;
                    }
                }
                else
                {
                    malwareWriteHeader("Устройство внезапно отключилась.");
                    mCmd = MalwareExecFail;
                    break;
                }
                malwareWriteLog("Распаковка");
                WAITMODE;

                num0 = mProgress;
                num1 = 0;
                for(const PackageIO &pkg : std::as_const(localPackages))
                {
                    int curValue = map<int, int>(num1, 0, localPackages.size(), num0, 48);
                    malwareWriteLog(QString(" >> md5 hash %1").arg(QString(QCryptographicHash::hash(pkg.packageName.toLatin1(), QCryptographicHash::Md5).toHex())), curValue);
                    ++num1;
                    WAITMODE2;
                }
                malwareWriteHeader("Получение данных завершена успешно.");
                WAITMODE;
                malwareWriteHeader("Анализ и обработка фоновых данных...");
                Adb::killPackages(adbDeviceSerial, localPackages, lastResult);
                if(localPackages.count() != lastResult)
                    malwareWriteLog("Частично.");
                else
                    malwareWriteLog("Выполнено.");
                WAITMODE;

                QStringList resultList {}, disableList {}, localPackageNames;
                LabStatusInfo labs;
                QString mdKey;
                std::transform(localPackages.begin(), localPackages.end(), std::back_inserter(localPackageNames), [](const PackageIO &package) { return package.packageName; });
                QObject::connect(
                    network,
                    &Network::sUploadUserPackages,
                    [&lastResult, &labs, &loop](int status, const LabStatusInfo &labsResult, bool ok)
                    {
                        if(ok)
                        {
                            labs = labsResult;
                        }
                        lastResult = status;
                        loop.quit();
                    });
                if(!network->pushUserPackages(device, localPackageNames))
                {
                    malwareWriteHeader("Ошибка при отправке");
                    malwareWriteLog(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }
                malwareWriteLog("Отправка образцов на сервер imister.kz и получение md-ключа", 49);
                loop.exec();
                mdKey = labs.mdKey;
                WAITMODE;

                if(lastResult)
                {
                    malwareWriteHeader("Ошибка во время загрузки");
                    malwareWriteLog(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }

                // Test MDKey
                QObject::connect(
                    network,
                    &Network::sFetchingLabs,
                    [&labs, &lastResult, &loop](int status, const LabStatusInfo &labsResult, bool ok)
                    {
                        if(ok)
                        {
                            labs = labsResult;
                        }
                        lastResult = status;
                        loop.quit();
                    });

                if(lastResult)
                {
                    malwareWriteHeader("Ошибка проверки ключа");
                    malwareWriteLog(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }

                int chances = 5;
                if(!labs.ready())
                {
                    malwareWriteHeader(
                        "Выполняется серверная обработка. Ожидаем. Если пройдет больше "
                        "времени, то можно попробовать позже.");
                    malwareWriteLog(
                        "В данный момент ожидается выполнение необходимых "
                        "действий администратора для продолжения. Ожидайте.");
                }
                mProgress = 49;
                while(labs.exists() && !labs.ready() && chances > 0)
                {
                    if(!check_device_connect())
                    {
                        malwareWriteHeader("Устройство внезапно отключилась.");
                        mCmd = MalwareExecFail;
                        break;
                    }

                    network->pullLabState(mdKey);
                    loop.exec();

                    WAITMODE2;

                    if(lastResult)
                    {
                        if(lastResult == NetworkStatus::ServerError)
                            break;
                        --chances;
                    }
                    else
                    {
                        chances = 5;
                    }
                }
                mProgress = 50;
                WAITMODE;

                if(lastResult)
                {
                    malwareWriteHeader("Возникла ошибка.");
                    malwareWriteLog(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }

                if(!network->authedId.hasVipAccount() && !labs.purchased)
                {
                    mUserValue = 1000;
                    while(mUserValue == 1000)
                    {
                        WAIT(100);
                    }

                    if(mUserValue != QMessageBox::StandardButton::Yes)
                    {
                        malwareWriteHeader("Запрос отклонен пользователем");
                        malwareWriteLog("вы отказались от оплаты");
                        WAIT(1000);
                        mCmd = MalwareExecFail;
                        break;
                    }
                }

                if(labs.analyzeStatus == "part-verify")
                {
                    malwareWriteHeader("АВТОМАТИЧЕСКИЙ РЕЖИМ (BETA) -- ВЫПОЛНЕНИЕ");
                    WAIT(2500);
                }

                malwareWriteLog(QString("md-ключ получен ") + labs.mdKey, 51);
                malwareWriteLog("Применение md-ключа и получение лабараторного анализа.", 52);
                WAITMODE;

                resultList.clear();
                disableList.clear();
                QObject::connect(
                    network,
                    &Network::sLabAdsFinish,
                    [&](int status, const AdsInfo &adsData, bool ok)
                    {
                        (void) lastResult;
                        (void) loop;
                        if(ok)
                        {
                            labs = adsData.labs;
                            resultList = adsData.blacklist;
                            disableList = adsData.disabling;
                        }
                        lastResult = status;
                        loop.quit();
                    });
                network->pullAdsData(labs.mdKey);
                loop.exec();

                if(lastResult != NetworkStatus::OK)
                {
                    malwareWriteHeader("Ошибка во время получения.");
                    malwareWriteLog(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }
                malwareWriteHeader("Результаты из лаборатории получены.", 55);
                WAITMODE;

                PRINT_LINE;
                if(resultList.isEmpty() && disableList.isEmpty())
                {
                    malwareWriteHeader("Действий не требуется");
                    malwareWriteLog("К сожалению, автоматический режим не обнаружил вредоносного ПО.");
                    malwareWriteLog(
                        "Если считаете что на устройстве все еще есть "
                        "рекламные вирусы, пожалуйста свяжитесь с нами.");
                    malwareWriteLog(
                        "Администратор проверит ваше устройство на наличие "
                        "рекламных вирусов.",
                        100);
                    mCmd++;
                    WAITMODE;
                    break;
                }

                malwareWriteLog("Распаковка", 57);
                resultList = compare_list(localPackageNames, resultList, [](const auto &lhs, const auto &rhs) -> bool { return lhs == rhs; });
                // resultList = existencePackages(localPackageNames,resultList);
                disableList = compare_list(localPackages, disableList, [](const auto &lhs, const auto &rhs) -> bool { return !lhs.disabled && lhs.packageName == rhs; });
                // disableList = existencePackageDisables(localPackages,disableList);
                totalMalwareDetected = static_cast<int>(resultList.size() + disableList.size());
                WAITMODE;

                num0 = mProgress;
                num1 = 0;
                for(const QString &pkg : std::as_const(resultList))
                {
                    ++num1;
                    // HAVE
                    int curValue = map<int, int>(num1, 0, resultList.size(), num0, 80);
                    malwareWriteLog(QString(" >> md5 hash %1").arg(QString(QCryptographicHash::hash(pkg.toLatin1(), QCryptographicHash::Md5).toHex())), curValue);
                    WAITMODE2;
                }
                WAITMODE;

                malwareWriteHeader("Обезвреживание устройства. Ждите...", 81);

                lastResult = 0;
                num1 = static_cast<int>(resultList.size());
                if(!resultList.isEmpty() && !Adb::uninstallPackages(adbDeviceSerial, resultList, lastResult))
                {
                    malwareWriteHeader("Ошибка");
                    malwareWriteLog("Что-то пошло не так. Возможно устройство было отключено.");
                    malwareWriteLog("Пожалуйста начните процедуру заново.");
                    WAITMODE;
                    mCmd = MalwareExecFail;
                    break;
                }
                num0 = lastResult;
                if(!disableList.isEmpty())
                {
                    num1 += static_cast<int>(disableList.size());
                    if(!Adb::disablePackages(adbDeviceSerial, disableList, lastResult))
                    {
                        malwareWriteHeader("Ошибка");
                        malwareWriteLog("Что-то пошло не так. Возможно устройство было отключено.");
                        malwareWriteLog("Пожалуйста начните процедуру заново.");
                        WAITMODE;
                        mCmd = MalwareExecFail;
                        break;
                    }
                    num0 += lastResult;
                }

                mProgress = map<int, int>(num0, 1, num1, mProgress, 99);
                if(num0 != num1)
                    malwareWriteLog("Предупреждение: Процесс частично успешен. Повторите.");
                else
                    mProgress = 99;
                mCmd++;
                break;
            }
            default:
                PRINT_LINE;

                WAIT(2000);

                isFinish = 1;
                if(mCmd < 0)
                {
                    malwareWriteHeader("Процедура завершилась ошибкой. Повторите.");
                    status = MalwareStatus::Error;
                }
                else
                {
                    QString text = "Процедура завершена.";
                    auto durationProcedure = duration_cast<seconds>(steady_clock::now() - procedureStartAt);
                    if(totalMalwareDetected)
                    {
                        text += " Рекламных вирусов удалено %1.";
                        text = text.arg(totalMalwareDetected);
                    }
                    text += QString(" Затрачено времени %1 с").arg(durationProcedure.count());

                    malwareWriteHeader(text, 100);
                    status = MalwareStatus::Idle;
                }
                break;
        }
    }
}

#undef WAITMODE
#undef FORCLYQUIT_CHECK
#undef WAIT
#undef WAITMODE
#undef WAITMODE2
#undef PRINT_LINE
