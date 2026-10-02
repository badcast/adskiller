#pragma once

#include <memory>

#include <QCryptographicHash>
#include <QDateEdit>
#include <QDesktopServices>
#include <QEventLoop>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QProgressBar>
#include <QPushButton>
#include <QStringListModel>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "AdbFront.h"
#include "AppleFront.h"
#include "Extension.h"
#include "Network.h"

#if !NDEBUG
#define SHOW_SERVICE_BY_DEBUG 0
#endif

constexpr auto DefaultIconWidget = "unavailable";

constexpr auto IDServiceAdsString = "0b9d1650-7a10-4fd5-a10e-53fc7f185b1b";
constexpr auto IDServiceMyDeviceString = "3db562cd-e448-4fc4-aeea-bc13f74ce5c9";
constexpr auto IDServiceAPKManagerString = "7193decc-f630-4d46-84cf-49059d9f4df5";
constexpr auto IDServiceStorageCleanString = "2ab13aa9-5051-4167-a024-3fbdcde11792";
constexpr auto IDServiceBoostRamString = "be1f68f6-0f91-4472-947a-07dbe313ab73";
constexpr auto IDServiceWhatsAppMoveString = "95bdb8a2-06f9-4d00-9625-a2da334001e6";
constexpr auto IDServiceContactFixerString = "578f74ec-2453-4b6c-8db4-cbb92175d437";
constexpr auto IDServiceMiUnlockString = "b05da077-dd39-4b70-980b-1b25379ec04a";
constexpr auto IDServiceVIPBuyString = "3a8b33fa-f2b0-4c09-87fe-84c828565731";
constexpr auto IDServiceAIAgentString = "039bc49d-6bdc-482b-a55e-1b6e8f73ea64";
constexpr auto IDServiceFileManagerString = "44b598b1-a969-42fa-8192-d59e2522542b";
constexpr auto IDServiceAITranslaterString = "92bcdf30-c410-4a0b-88f9-516c29f7ee8a";
constexpr auto IDServiceAppleIpswString = "a9f1b2c3-4d5e-6f7a-8b9c-0d1e2f3a4b5c";
constexpr auto IDServiceImeiVerificationKzString = "ef469f7e-6aa3-4f3e-8851-9ae03e418237";
constexpr auto IDServiceMiAccountString = IDServiceMiUnlockString;
constexpr auto IDServiceSamsungFrpString = "88117553-ba0c-45bd-b80c-155e42a7fe7e";

namespace Generic
{
    QString formatSizes(qint64 bytes);

    QPixmap generateFallbackIcon(const QString &appName, const QString &pkgName, bool isSystem);

} // namespace Generic

enum PageIndex
{
    AuthPage = 0,
    LoaderPage,
    CabinetPage,
    LongInfoPage,
    DevicesPage,
    MyDevicesPage,
    BuyVIPPage,
    FileManagerPage,
    ApkManagerPage,
    ContactFixerPage,
    AITranslaterPage,
    AppleIpswPage,
    StorageCacheCleanPage,
    ImeiVerificationKzPage,
    MiAccountPage,
    SamsungFrpPage,

    LengthPages
};

enum class ServiceFlag
{
    None = 0,
    New = 1 << 0,
    Beta = 1 << 1,
    NewBeta = (1 << 0) | (1 << 1)
};

inline constexpr ServiceFlag operator|(ServiceFlag a, ServiceFlag b)
{
    return static_cast<ServiceFlag>(static_cast<int>(a) | static_cast<int>(b));
}

inline constexpr ServiceFlag operator&(ServiceFlag a, ServiceFlag b)
{
    return static_cast<ServiceFlag>(static_cast<int>(a) & static_cast<int>(b));
}

class Service;
class UnavailableService;
class AdsKillerService;
class StorageCacheCleanService;
class BuyVIPService;
class ApkManagerService;
class ContactFixerService;
class MiDeviceUnlockService;
class FileManagerService;
class AITranslaterService;
class AppleIpswService;
class ImeiVerificationKzService;
class SamsungFrpService;
class ServiceProvider;

class ServiceProvider
{
    friend Service;
    friend MainWindow;

public:
    ServiceProvider() = delete;
    ~ServiceProvider() = delete;

    static bool runService(std::shared_ptr<Service> service);
    static void closeService();
    static std::shared_ptr<Service> currentService();
};

class Service : public QObject
{
    Q_OBJECT

public:
    using Flag = ServiceFlag;

protected:
    DeviceConnectType mDeviceConnectType;
    AdbDevice mAdbDevice;
    AppleDevice mAppleDevice;
    int m_sortScore = 100;
    ServiceFlag m_flag = ServiceFlag::None;

public:
    QString title;
    QWidget *ownerWidget;
    bool active;

    inline Service(DeviceConnectType deviceConnectType, QObject *parent = nullptr) : QObject(parent), ownerWidget(nullptr), title(), active(false), mDeviceConnectType(deviceConnectType), m_sortScore(100), m_flag(ServiceFlag::None)
    {
    }

    virtual void setAndroidArgs(const AdbDevice &adbDevice);
    virtual void setAppleArgs(const AppleDevice &appleDevice);
    const AppleDevice &appleDevice() const
    {
        return mAppleDevice;
    }
    const AdbDevice &adbDevice() const
    {
        return mAdbDevice;
    }

    virtual int sort_score() const
    {
        return m_sortScore;
    }
    void setSortScore(int score)
    {
        m_sortScore = score;
    }

    virtual ServiceFlag flag() const
    {
        return m_flag;
    }
    void setFlag(ServiceFlag flag)
    {
        m_flag = flag;
    }

    virtual QString uuid() const = 0;
    virtual bool isAvailable() const;
    virtual PageIndex targetPage();
    virtual bool canStart();
    virtual bool isStarted() = 0;
    virtual bool isFinish() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual QString widgetIconName();

    bool restart();
    void close();

    DeviceConnectType deviceConnectType() const;
    virtual bool isOnlineService() const;
    virtual void sendCheckPull() const;

public:
    static std::list<std::shared_ptr<Service>> EnumAppServices(QObject *parent = nullptr);
};

class UnavailableService : public Service
{
    Q_OBJECT

public:
    UnavailableService(QObject *parent = nullptr);

    int sort_score() const override
    {
        return 0;
    }

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
};

class AdsKillerService : public Service
{
    Q_OBJECT

private:
    QListView *processLogStatus;
    QLabel *malwareStatusText0;
    QLabel *deviceLabelName;
    QProgressBar *processBarStatus;
    QPushButton *pushButtonReRun;

    void cirlceMalwareState(bool success);
    void cirlceMalwareStateReset();

public slots:
    void onPullServiceUUID(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

public:
    struct PrivateKillerRes *_priv;
    AdsKillerService(QObject *parent = nullptr);
    ~AdsKillerService();

    int sort_score() const override
    {
        return 500;
    }

    void setAndroidArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;
};

class StorageCacheCleanWidget;

class StorageCacheCleanService : public Service
{
    Q_OBJECT

public:
    StorageCacheCleanService(QObject *parent = nullptr);
    ~StorageCacheCleanService() override;

    int sort_score() const override
    {
        return 101;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::New;
    }

    void setAndroidArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

class BuyVIPService : public Service
{
    Q_OBJECT

public:
    BuyVIPService(QObject *parent = nullptr);
    ~BuyVIPService();

    int sort_score() const override
    {
        return 100;
    }

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    PageIndex targetPage() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private slots:
    void click_buy_vip();
    void variant_selected();
    void service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

private:
    Network *network;
    QComboBox *listVariants;
    QLabel *balanceText;
    QLabel *infoAfterPeriod;
    QPushButton *buyButton;
    std::uint32_t dailyRate;
    int mind, maxd;
    QList<std::tuple<QString, int>> mPresets;
};

class MyDeviceService : public Service
{
    Q_OBJECT
public:
    MyDeviceService(QObject *parent = nullptr);
    ~MyDeviceService();

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

public slots:
    void slotPullMyDeviceList(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok);

    void slotQuaranteeUpdate();
    void slotRefresh();

private:
    int mInternalData;
    QTableView *table;
    QDateEdit *dateEditBegin;
    QDateEdit *dateEditEnd;
    QPushButton *refreshButton;
    QCheckBox *quaranteeFilter;
    std::shared_ptr<QList<DeviceItemInfo>> actual;
    std::shared_ptr<QList<DeviceItemInfo>> expired;

    void clearMyDevicesPage(QString text);
    void fillMyDevicesPage();
};

class BoostRamService : public Service
{
    Q_OBJECT

private:
    QListView *processLogStatus;
    QLabel *malwareStatusText0;
    QLabel *deviceLabelName;
    QProgressBar *processBarStatus;
    QPushButton *pushButtonReRun;

    void circleRamState(bool success);
    void circleRamStateReset();

public:
    BoostRamService(QObject *parent = nullptr);
    ~BoostRamService();

    int sort_score() const override
    {
        return 101;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::New;
    }

    void setAndroidArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;
};

class ContactFixerService : public Service
{
    Q_OBJECT

private:
    struct CFSInternalData *mInternal;

public:
    ContactFixerService(QObject *parent = nullptr);
    ~ContactFixerService();

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;
};

class MiAccountWidget;

class MiDeviceUnlockService : public Service
{
    Q_OBJECT

public:
    MiDeviceUnlockService(QObject *parent = nullptr);
    ~MiDeviceUnlockService() override;

    int sort_score() const override
    {
        return 200;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::NewBeta;
    }

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

using MiAccountService = MiDeviceUnlockService;

class AIAgentService : public Service
{
    Q_OBJECT

    void slotPullMessage(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

public:
    AIAgentService(QObject *parent = nullptr);
    ~AIAgentService();

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

    static void resetHistory();

public slots:
    void sendCurrentMessage();

Q_SIGNALS:
    void onRunService(QString service_uuid);

public:
    QStringList aiMessages;
    int aiSessionId = -1;
    // Typing indicator state
    int aiTypingId = 0;
    QString aiTypingSpanId;
    int aiTypingDots = 0;
    QTimer *aiTypingTimer = nullptr;
};

class FileManagerService : public Service
{
    Q_OBJECT

public:
    FileManagerService(QObject *parent = nullptr);
    ~FileManagerService() override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

class ApkManagerService : public Service
{
    Q_OBJECT

public:
    ApkManagerService(QObject *parent = nullptr);
    ~ApkManagerService() override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

class AITranslaterService : public Service
{
    Q_OBJECT

private:
    struct ATSInternalData *mInternal;

public:
    AITranslaterService(QObject *parent = nullptr);
    ~AITranslaterService();

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;
};

class AppleIpswService : public Service
{
    Q_OBJECT

public:
    AppleIpswService(QObject *parent = nullptr);
    ~AppleIpswService() override;

    int sort_score() const override
    {
        return 300;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::Beta;
    }

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

class ImeiVerificationKzWidget;

class ImeiVerificationKzService : public Service
{
    Q_OBJECT

public:
    ImeiVerificationKzService(QObject *parent = nullptr);
    ~ImeiVerificationKzService() override;

    int sort_score() const override
    {
        return 250;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::NewBeta;
    }

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};

class SamsungFrpWidget;

class SamsungFrpService : public Service
{
    Q_OBJECT

public:
    SamsungFrpService(QObject *parent = nullptr);
    ~SamsungFrpService() override;

    int sort_score() const override
    {
        return 180;
    }

    ServiceFlag flag() const override
    {
        return ServiceFlag::NewBeta;
    }

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    QString widgetIconName() override;

private:
    bool m_started = false;
    bool m_finished = false;
};
