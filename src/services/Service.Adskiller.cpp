#include <tuple>
#include <memory>
#include <functional>

#include "Services.h"
#include "mainwindow.h"

#define FORCLYQUIT_CHECK             \
    if(mCmd == CommandExecForceKill) \
    break
#define WAIT(MS) QThread::msleep(MS)
#define WAITMODE      \
    FORCLYQUIT_CHECK; \
    WAIT(400)
#define WAITMODE2     \
    FORCLYQUIT_CHECK; \
    WAIT(50)
#define PRINT_LINE adskiller_write_log("--------------------------------------------------------")

constexpr auto NET_COMMAND_GETADS = "GETADS";
constexpr auto NET_COMMAND_MDKEYSTATE = "MDKEYSTATUS";
constexpr auto NET_COMMAND_UPLOADPACKAGES = "UPLOADPKGS";

enum
{
    CommandExecFailed = -1,
    CommandExecForceKill = -2
};

enum MalwareStatus
{
    Idle = 0,
    Running,
    Error
};

struct PrivateKillerRes
{
    int lastNetStatus;
    std::shared_ptr<AdsInfo> adsdata;
    std::shared_ptr<LabStatusInfo> labInfo;
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

void adskiller_kill_proc();
bool adskiller_clean_cmd();
void adskiller_awake(AdsKillerService *servive);
void adskiller_user_confirm(int userValue);

inline LabStatusInfo fromJsonLabs(const QJsonValue &jroot)
{
    LabStatusInfo retval {};
    if(jroot.isObject() && jroot["analyzeStatus"].isString() && jroot["mdKey"].isString())
    {
        retval.analyzeStatus = jroot["analyzeStatus"].toString();
        retval.mdKey = jroot["mdKey"].toString();
        retval.purchased = jroot["purchased"].toBool();
    }
    return retval;
}

inline void adskiller_user_confirm(int userValue)
{
    QMutexLocker locker(mutex);
    (void) locker;
    mUserValue = userValue;
}

inline std::pair<QStringList, int> adskiller_read_log()
{
    QMutexLocker locker(mutex);
    (void) locker;
    return {QStringList(std::move(outLogs)), mProgress};
}

inline QString adskiller_read_head()
{
    QMutexLocker locker(mutex);
    (void) locker;
    return outHeads;
}

inline void adskiller_write_log(QString msg, int progress = -1)
{
    QMutexLocker locker(mutex);
    (void) locker;
    outLogs << std::move(msg.split('\n'));
    if(progress > -1)
        mProgress = progress;
}

inline void adskiller_write_log_head(QString msg, int progress = -1)
{
    QMutexLocker locker(mutex);
    (void) locker;
    outHeads = std::move(msg);
    outLogs << outHeads;
    if(progress > -1)
        mProgress = progress;
}

QString AdsKillerService::uuid() const
{
    return IDServiceAdsString;
}

QString AdsKillerService::widgetIconName()
{
    return "white-ads-remove";
}

AdsKillerService::AdsKillerService(QObject *parent) : Service(DeviceConnectType::ADB, parent), processLogStatus(nullptr), malwareStatusText0(nullptr), deviceLabelName(nullptr), processBarStatus(nullptr), pushButtonReRun(nullptr), _priv(new PrivateKillerRes)
{
}

AdsKillerService::~AdsKillerService()
{
    if(_priv)
    {
        delete _priv;
        _priv = nullptr;
    }
}

void AdsKillerService::setArgs(const AdbDevice &adbDevice)
{
    Service::setArgs(adbDevice);
    MainWindow::current->accessUi_page_longinfo(processLogStatus, malwareStatusText0, deviceLabelName, processBarStatus, pushButtonReRun);
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

bool AdsKillerService::start()
{
    if(!canStart())
        return false;
    pushButtonReRun->setEnabled(false);

    QStringListModel *model = static_cast<QStringListModel *>(processLogStatus->model());
    QStringList place = model->stringList();
    QString deviceName = QString("Выбранное устройство (%1)").arg(mAdbDevice.displayName);

    cirlceMalwareStateReset();

    processBarStatus->setValue(0);
    MainWindow::current->malwareProgressCircle->setInfinilyMode(true);
    MainWindow::current->malwareProgressCircle->setValue(0);

    deviceLabelName->setText(deviceName);

    place << "<< Запуск процесса удаления рекламы, пожалуйста подождите >>";
    place << "<< Не отсоединяйте устройство от компьютера >>";
    model->setStringList(place);

    MainWindow::current->delayUICall(
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
                    header = adskiller_read_head();
                    reads = adskiller_read_log();

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
                        adskiller_clean_cmd();
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
                        adskiller_user_confirm(num0);
                    }
                });
        });

    if(malwareThread != nullptr)
    {
        adskiller_write_log("Процесс уже запущен.");
        return false;
    }

    mutex = new QMutex();
    malwareThread = new QThread();
    QObject::connect(malwareThread, &QThread::started, std::bind(adskiller_awake, this));
    mCmd = 0;
    mProgress = 0;
    adbDeviceSerial = mAdbDevice.devId;
    status = MalwareStatus::Running;
    malwareThread->start();

    return isStarted();
}

void AdsKillerService::stop()
{
    if(isStarted())
        adskiller_kill_proc();
    if(isFinish())
        cirlceMalwareStateReset();
    if(pushButtonReRun)
        pushButtonReRun->setEnabled(true);
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
constexpr inline OutT Map(const InT x, const InT in_min, const InT in_max, const OutT out_min, const OutT out_max)
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

bool adskiller_clean_cmd()
{
    if(status == MalwareStatus::Running)
        return false;
    delete network;
    network = nullptr;
    delete mutex;
    mutex = nullptr;
    malwareThread = nullptr;
    return true;
}

void adskiller_kill_proc()
{
    if(malwareThread == nullptr)
        return;
    mCmd = CommandExecForceKill;
}

std::shared_ptr<AdsInfo> fetch_ads_data(AdsKillerService *service, const QString &mdKey)
{
    QJsonObject request;
    request["cmd"] = NET_COMMAND_GETADS;
    request["mdKey"] = mdKey;
    QEventLoop loop;
    QObject::connect(network, &Network::sPullServiceUUID, &loop, &QEventLoop::quit);
    network->pullServiceUUID(service->uuid(), request, ServiceOperation::Get);
    loop.exec();
    return std::move(service->_priv->adsdata);
}

std::shared_ptr<LabStatusInfo> fetch_lab_state(AdsKillerService *service, const QString &mdKey)
{
    QJsonObject request;
    request["cmd"] = NET_COMMAND_MDKEYSTATE;
    request["mdKey"] = mdKey;
    QEventLoop loop;
    QObject::connect(network, &Network::sPullServiceUUID, &loop, &QEventLoop::quit);
    network->pullServiceUUID(service->uuid(), request, ServiceOperation::Get);
    loop.exec();
    return std::move(service->_priv->labInfo);
}

std::shared_ptr<LabStatusInfo> fetch_device_packages(AdsKillerService *service, const AdbDevice &device, const QStringList &packages)
{
    QJsonArray array;
    QJsonObject request;
    QEventLoop loop;
    QObject::connect(network, &Network::sPullServiceUUID, &loop, &QEventLoop::quit);
    request["cmd"] = NET_COMMAND_UPLOADPACKAGES;
    request["deviceSerial"] = device.devId;
    request["deviceModel"] = device.model;
    request["deviceVendor"] = device.vendor;
    for(const QString &str : packages)
        array.append(str);
    request["packages"] = array;
    network->pullServiceUUID(service->uuid(), request, ServiceOperation::Open);
    loop.exec();
    return std::move(service->_priv->labInfo);
}

void adskiller_awake(AdsKillerService *service)
{
    using namespace std::chrono;

    int isFinish = 0;
    int &lastResult = service->_priv->lastNetStatus = 0;

    int num0, num1, totalMalwareDetected;
    QList<PackageIO> localPackages;
    std::shared_ptr<AdbSysInfo> sysInfo;

    network = new Network(MainWindow::current->network);

    QObject::connect(network, &Network::sPullServiceUUID, service, &AdsKillerService::onPullServiceUUID, Qt::DirectConnection);

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
            .arg(device.model, device.vendor, sysInfo->OSVersionString(), sysInfo->StorageDesignString(), sysInfo->RAMDesignString(), sysInfo->systemName, sysInfo->machine);
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
                adskiller_write_log_head("Запуск процедуры удаление рекламы (Malware)...", 1);
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
                    adskiller_write_log_head("Устройство внезапно отключилась.");
                    adskiller_write_log("Пожалуйста, убедитесь что устройство подключено корректно и кабель не поврежден.");
                    mCmd = CommandExecFailed;
                    break;
                }
                adskiller_write_log_head(QString("Получение данных с устройства ") + device.displayName + "(" + device.devId + ")", 2);
                sysInfo = AdbShell(device.devId).getInfo();
                adskiller_write_log(print_device_info());
                WAITMODE;

                localPackages = Adb::getPackages(adbDeviceSerial);
                if(check_device_connect())
                {
                    if(localPackages.isEmpty())
                    {
                        adskiller_write_log_head("Получили пустой результат.");
                        mCmd = CommandExecFailed;
                        break;
                    }
                }
                else
                {
                    adskiller_write_log_head("Устройство внезапно отключилась.");
                    mCmd = CommandExecFailed;
                    break;
                }
                adskiller_write_log("Распаковка");
                WAITMODE;

                num0 = mProgress;
                num1 = 0;
                for(const PackageIO &pkg : std::as_const(localPackages))
                {
                    int curValue = Map<int, int>(num1, 0, localPackages.size(), num0, 48);
                    adskiller_write_log(QString(" >> md5 hash %1").arg(QString(QCryptographicHash::hash(pkg.packageName.toLatin1(), QCryptographicHash::Md5).toHex())), curValue);
                    ++num1;
                    WAITMODE2;
                }
                adskiller_write_log_head("Получение данных завершена успешно.");
                WAITMODE;
                adskiller_write_log_head("Анализ и обработка фоновых данных...");
                Adb::killPackages(adbDeviceSerial, localPackages, lastResult);
                if(localPackages.count() != lastResult)
                    adskiller_write_log("Частично.");
                else
                    adskiller_write_log("Выполнено.");
                WAITMODE;

                QStringList resultList {}, disableList {}, localPackageNames;
                std::shared_ptr<LabStatusInfo> labs;
                QString mdKey;

                std::transform(localPackages.begin(), localPackages.end(), std::back_inserter(localPackageNames), [](const PackageIO &package) { return package.packageName; });

                labs = fetch_device_packages(service, device, localPackageNames);
                if(!labs)
                {
                    adskiller_write_log_head("Ошибка при отправке");
                    adskiller_write_log(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = CommandExecFailed;
                    break;
                }
                adskiller_write_log("Отправка образцов на сервер imister.kz и получение md-ключа", 49);

                mdKey = labs->mdKey;
                WAITMODE;

                if(lastResult)
                {
                    adskiller_write_log_head("Ошибка во время загрузки");
                    adskiller_write_log(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = CommandExecFailed;
                    break;
                }

                int chances = 5;
                if(!labs->ready())
                {
                    adskiller_write_log_head(
                        "Выполняется серверная обработка. Ожидаем. Если пройдет больше "
                        "времени, то можно попробовать позже.");
                    adskiller_write_log(
                        "В данный момент ожидается выполнение необходимых "
                        "действий администратора для продолжения. Ожидайте.");
                }
                mProgress = 49;
                while(labs && labs->exists() && !labs->ready() && chances > 0)
                {
                    if(!check_device_connect())
                    {
                        adskiller_write_log_head("Устройство внезапно отключилась.");
                        mCmd = CommandExecFailed;
                        break;
                    }

                    labs = fetch_lab_state(service, mdKey);

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
                    adskiller_write_log_head("Возникла ошибка.");
                    adskiller_write_log(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = CommandExecFailed;
                    break;
                }

                if(!network->authedId.hasVipAccount() && !labs->purchased)
                {
                    mUserValue = 1000;
                    while(mUserValue == 1000)
                    {
                        WAIT(100);
                    }

                    if(mUserValue != QMessageBox::StandardButton::Yes)
                    {
                        adskiller_write_log_head("Запрос отклонен пользователем");
                        adskiller_write_log("вы отказались от оплаты");
                        WAIT(1000);
                        mCmd = CommandExecFailed;
                        break;
                    }
                }

                if(labs->analyzeStatus == "part-verify")
                {
                    adskiller_write_log_head("АВТОМАТИЧЕСКИЙ РЕЖИМ (BETA) -- ВЫПОЛНЕНИЕ");
                    WAIT(2500);
                }

                adskiller_write_log(QString("md-ключ получен ") + labs->mdKey, 51);
                adskiller_write_log("Применение md-ключа и получение лабараторного анализа.", 52);
                WAITMODE;

                resultList.clear();
                disableList.clear();

                std::shared_ptr<AdsInfo> ads_data_input = fetch_ads_data(service, labs->mdKey);
                if(lastResult != NetworkStatus::OK || !ads_data_input)
                {
                    adskiller_write_log_head("Ошибка во время получения.");
                    adskiller_write_log(generate_error_report(lastResult));
                    WAITMODE;
                    mCmd = CommandExecFailed;
                    break;
                }

                labs = std::make_shared<LabStatusInfo>(ads_data_input->labs);
                resultList = ads_data_input->blacklist;
                disableList = ads_data_input->disabling;

                adskiller_write_log_head("Результаты из лаборатории получены.", 55);
                WAITMODE;

                PRINT_LINE;
                if(resultList.isEmpty() && disableList.isEmpty())
                {
                    adskiller_write_log_head("Действий не требуется");
                    adskiller_write_log("К сожалению, автоматический режим не обнаружил вредоносного ПО.");
                    adskiller_write_log(
                        "Если считаете что на устройстве все еще есть "
                        "рекламные вирусы, пожалуйста свяжитесь с нами.");
                    adskiller_write_log(
                        "Администратор проверит ваше устройство на наличие "
                        "рекламных вирусов.",
                        100);
                    mCmd++;
                    WAITMODE;
                    break;
                }

                adskiller_write_log("Распаковка", 57);
                resultList = compare_list(localPackageNames, resultList, [](const auto &lhs, const auto &rhs) -> bool { return lhs == rhs; });
                disableList = compare_list(localPackages, disableList, [](const auto &lhs, const auto &rhs) -> bool { return !lhs.disabled && lhs.packageName == rhs; });
                totalMalwareDetected = static_cast<int>(resultList.size() + disableList.size());
                WAITMODE;

                num0 = mProgress;
                num1 = 0;
                for(const QString &pkg : std::as_const(resultList))
                {
                    ++num1;
                    int curValue = Map<int, int>(num1, 0, resultList.size(), num0, 80);
                    adskiller_write_log(QString(" >> md5 hash %1").arg(QString(QCryptographicHash::hash(pkg.toLatin1(), QCryptographicHash::Md5).toHex())), curValue);
                    WAITMODE2;
                }
                WAITMODE;

                adskiller_write_log_head("Обезвреживание устройства. Ждите...", 81);

                lastResult = 0;
                num1 = static_cast<int>(resultList.size());
                if(!resultList.isEmpty() && !Adb::uninstallPackages(adbDeviceSerial, resultList, lastResult))
                {
                    adskiller_write_log_head("Ошибка");
                    adskiller_write_log("Что-то пошло не так. Возможно устройство было отключено.");
                    adskiller_write_log("Пожалуйста начните процедуру заново.");
                    WAITMODE;
                    mCmd = CommandExecFailed;
                    break;
                }
                num0 = lastResult;
                if(!disableList.isEmpty())
                {
                    num1 += static_cast<int>(disableList.size());
                    if(!Adb::disablePackages(adbDeviceSerial, disableList, lastResult))
                    {
                        adskiller_write_log_head("Ошибка");
                        adskiller_write_log("Что-то пошло не так. Возможно устройство было отключено.");
                        adskiller_write_log("Пожалуйста начните процедуру заново.");
                        WAITMODE;
                        mCmd = CommandExecFailed;
                        break;
                    }
                    num0 += lastResult;
                }

                mProgress = Map<int, int>(num0, 1, num1, mProgress, 99);
                if(num0 != num1)
                    adskiller_write_log("Предупреждение: Процесс частично успешен. Повторите.");
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
                    adskiller_write_log_head("Процедура завершилась ошибкой. Повторите.");
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

                    adskiller_write_log_head(text, 100);
                    status = MalwareStatus::Idle;
                }
                break;
        }
    }
}

void AdsKillerService::onPullServiceUUID(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok)
{
    QJsonArray jarray;
    QString cmd;

    _priv->lastNetStatus = 1;
    while(true)
    {
        if(ok)
        {
            cmd = responce["cmd"].toString();
            if(cmd == NET_COMMAND_GETADS)
            {
                _priv->adsdata = std::make_shared<AdsInfo>();
                _priv->adsdata->labs = fromJsonLabs(responce["labs"]);
                jarray = responce["result"].toArray();
                for(const QJsonValue &val : std::as_const(jarray))
                    _priv->adsdata->blacklist << val.toString();
                jarray = responce["autodisable"].toArray();
                if(!responce["autodisable"].isNull())
                    for(const QJsonValue &val : std::as_const(jarray))
                        _priv->adsdata->disabling << val.toString();
            }
            else if(cmd == NET_COMMAND_MDKEYSTATE || cmd == NET_COMMAND_UPLOADPACKAGES)
            {
                _priv->labInfo = std::make_shared<LabStatusInfo>(fromJsonLabs(responce["labs"]));
            }
            else
            {
                break;
            }
            _priv->lastNetStatus = 0;
        }
        break;
    }
}
#undef WAITMODE
#undef FORCLYQUIT_CHECK
#undef WAIT
#undef WAITMODE
#undef WAITMODE2
#undef PRINT_LINE
