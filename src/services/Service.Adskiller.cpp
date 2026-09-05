#include <tuple>
#include <memory>
#include <functional>

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>

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

class PurchaseConfirmDialog : public QDialog
{
public:
    PurchaseConfirmDialog(QWidget *parent, const QString &deviceName, const UserDataInfo &data)
        : QDialog(parent)
    {
        setWindowTitle("Подтверждение покупки — AdsKiller");
        setWindowIcon(QIcon(":/resources/app-logo"));
        setModal(true);
        setFixedWidth(460);

        setStyleSheet(
            "QDialog {"
            "   background-color: #1A1C20;"
            "   color: #F3F4F6;"
            "}"
        );

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(22, 20, 22, 20);
        mainLayout->setSpacing(16);

        // Header with icon and title
        QHBoxLayout *headerLayout = new QHBoxLayout();
        headerLayout->setSpacing(14);

        QLabel *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(48, 48);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setText("🛡️");
        iconLabel->setStyleSheet(
            "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1E3A5F, stop:1 #0F2038);"
            "border: 1px solid #2563EB;"
            "border-radius: 24px;"
            "font-size: 22px;"
        );
        headerLayout->addWidget(iconLabel);

        QVBoxLayout *titleLayout = new QVBoxLayout();
        titleLayout->setSpacing(3);

        QLabel *titleLabel = new QLabel("Подтверждение оплаты", this);
        titleLabel->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold;");
        titleLayout->addWidget(titleLabel);

        QLabel *subtitleLabel = new QLabel("Очистка и обезвреживание рекламы", this);
        subtitleLabel->setStyleSheet("color: #9CA3AF; font-size: 12px; font-weight: 500;");
        titleLayout->addWidget(subtitleLabel);

        headerLayout->addLayout(titleLayout);
        headerLayout->addStretch();
        mainLayout->addLayout(headerLayout);

        // Details Card
        QFrame *cardFrame = new QFrame(this);
        cardFrame->setObjectName("purchaseDetailsCard");
        cardFrame->setStyleSheet(
            "QFrame#purchaseDetailsCard {"
            "   background-color: #141518;"
            "   border: 1px solid #282B32;"
            "   border-radius: 10px;"
            "}"
        );

        QGridLayout *cardGrid = new QGridLayout(cardFrame);
        cardGrid->setContentsMargins(16, 14, 16, 14);
        cardGrid->setVerticalSpacing(10);
        cardGrid->setHorizontalSpacing(16);

        QString cleanDevName = deviceName;
        const QString prefix = "Выбранное устройство (";
        if(cleanDevName.startsWith(prefix) && cleanDevName.endsWith(")"))
        {
            cleanDevName = cleanDevName.mid(prefix.length(), cleanDevName.length() - prefix.length() - 1).trimmed();
        }
        if(cleanDevName.isEmpty())
            cleanDevName = deviceName;

        // Row 0: Device
        QLabel *lblDevTitle = new QLabel("📱 Устройство:", cardFrame);
        lblDevTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
        cardGrid->addWidget(lblDevTitle, 0, 0);

        QLabel *lblDevVal = new QLabel(cleanDevName, cardFrame);
        lblDevVal->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: bold;");
        lblDevVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardGrid->addWidget(lblDevVal, 0, 1);

        // Row 1: Procedure
        QLabel *lblProcTitle = new QLabel("📋 Процедура:", cardFrame);
        lblProcTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
        cardGrid->addWidget(lblProcTitle, 1, 0);

        QLabel *lblProcVal = new QLabel("Удаление рекламы и вредоносов", cardFrame);
        lblProcVal->setStyleSheet("color: #E5E7EB; font-size: 12px; font-weight: 500;");
        lblProcVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardGrid->addWidget(lblProcVal, 1, 1);

        // Row 2: Divider
        QFrame *divider = new QFrame(cardFrame);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet("background-color: #262930; max-height: 1px;");
        cardGrid->addWidget(divider, 2, 0, 1, 2);

        // Row 3: Price
        QLabel *lblPriceTitle = new QLabel("💰 Стоимость:", cardFrame);
        lblPriceTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
        cardGrid->addWidget(lblPriceTitle, 3, 0);

        QLabel *lblPriceVal = new QLabel(QString("%1 %2").arg(data.basePrice).arg(data.currencyType), cardFrame);
        lblPriceVal->setStyleSheet("color: #4CC2FF; font-size: 14px; font-weight: bold;");
        lblPriceVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardGrid->addWidget(lblPriceVal, 3, 1);

        // Row 4: Current balance
        QLabel *lblBalanceTitle = new QLabel("💳 Текущий баланс:", cardFrame);
        lblBalanceTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
        cardGrid->addWidget(lblBalanceTitle, 4, 0);

        QLabel *lblBalanceVal = new QLabel(QString("%1 %2").arg(data.credits).arg(data.currencyType), cardFrame);
        lblBalanceVal->setStyleSheet("color: #FBBF24; font-size: 13px; font-weight: bold;");
        lblBalanceVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardGrid->addWidget(lblBalanceVal, 4, 1);

        // Row 5: Balance after purchase
        int remainingCredits = qMax<int>(0, static_cast<int>(data.credits) - static_cast<int>(data.basePrice));
        bool hasEnough = (data.credits >= data.basePrice);

        QLabel *lblAfterTitle = new QLabel("📊 Баланс после списания:", cardFrame);
        lblAfterTitle->setStyleSheet("color: #9CA3AF; font-size: 12px;");
        cardGrid->addWidget(lblAfterTitle, 5, 0);

        QLabel *lblAfterVal = new QLabel(cardFrame);
        if(hasEnough)
        {
            lblAfterVal->setText(QString("%1 %2").arg(remainingCredits).arg(data.currencyType));
            lblAfterVal->setStyleSheet("color: #34D399; font-size: 13px; font-weight: bold;");
        }
        else
        {
            lblAfterVal->setText("Недостаточно средств");
            lblAfterVal->setStyleSheet("color: #F87171; font-size: 12px; font-weight: bold;");
        }
        lblAfterVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cardGrid->addWidget(lblAfterVal, 5, 1);

        mainLayout->addWidget(cardFrame);

        // Warning banner if insufficient funds
        if(!hasEnough)
        {
            QLabel *warnLabel = new QLabel("⚠️ Недостаточно средств на балансе для оплаты процедуры. Пожалуйста, пополните баланс через меню «Поддержка → Связаться».", this);
            warnLabel->setWordWrap(true);
            warnLabel->setStyleSheet(
                "background-color: rgba(239, 68, 68, 0.12);"
                "border: 1px solid rgba(239, 68, 68, 0.35);"
                "border-radius: 6px;"
                "color: #FCA5A5;"
                "font-size: 11px;"
                "padding: 8px 10px;"
            );
            mainLayout->addWidget(warnLabel);
        }

        // Action Buttons
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->setSpacing(12);

        QPushButton *cancelBtn = new QPushButton("Отмена", this);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setMinimumHeight(38);
        cancelBtn->setStyleSheet(
            "QPushButton {"
            "   background-color: #22252B;"
            "   border: 1px solid #32363E;"
            "   border-radius: 8px;"
            "   color: #D1D5DB;"
            "   font-size: 13px;"
            "   font-weight: 600;"
            "   padding: 0px 20px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #2D3139;"
            "   border-color: #4B5563;"
            "   color: #FFFFFF;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #1A1C20;"
            "}"
        );
        btnLayout->addWidget(cancelBtn);

        QPushButton *confirmBtn = new QPushButton(this);
        confirmBtn->setCursor(Qt::PointingHandCursor);
        confirmBtn->setMinimumHeight(38);
        if(hasEnough)
        {
            confirmBtn->setText(QString("💳 Оплатить %1 %2").arg(data.basePrice).arg(data.currencyType));
            confirmBtn->setEnabled(true);
        }
        else
        {
            confirmBtn->setText("Недостаточно средств");
            confirmBtn->setEnabled(false);
        }
        confirmBtn->setStyleSheet(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10B981, stop:1 #059669);"
            "   border: 1px solid #059669;"
            "   border-radius: 8px;"
            "   color: #FFFFFF;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 0px 24px;"
            "}"
            "QPushButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #34D399, stop:1 #10B981);"
            "   border-color: #34D399;"
            "}"
            "QPushButton:pressed {"
            "   background: #047857;"
            "}"
            "QPushButton:disabled {"
            "   background-color: #2A2E35;"
            "   border-color: #363A42;"
            "   color: #6B7280;"
            "}"
        );
        confirmBtn->setDefault(true);
        btnLayout->addWidget(confirmBtn);

        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(confirmBtn, &QPushButton::clicked, this, &QDialog::accept);

        mainLayout->addLayout(btnLayout);
    }

    static int confirm(QWidget *parent, const QString &deviceName, const UserDataInfo &data)
    {
        PurchaseConfirmDialog dlg(parent, deviceName, data);
        if(dlg.exec() == QDialog::Accepted)
            return QMessageBox::StandardButton::Yes;
        return QMessageBox::StandardButton::No;
    }
};

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

    AdbDevice dev = mAdbDevice;
    if(dev.isEmpty() && !adbDeviceSerial.isEmpty())
        dev = Adb::getDevice(adbDeviceSerial);

    std::shared_ptr<AdbSysInfo> initialInfo = AdbShell(dev.devId).getInfo();
    deviceLabelName->setText(generateDeviceDashboardHtml(dev, initialInfo));

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
                        malwareUpdateTimer->stop();
                        UserDataInfo data = MainWindow::current->network.authedId;
                        int num0 = PurchaseConfirmDialog::confirm(MainWindow::current, deviceName, data);
                        adskiller_user_confirm(num0);
                        if(status == MalwareStatus::Running)
                            malwareUpdateTimer->start(100);
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
    malwareThread->quit();
    malwareThread->wait();
    delete malwareThread;
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
                adskiller_write_log("Отправка образцов на сервер imister.tech и получение md-ключа", 49);

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
