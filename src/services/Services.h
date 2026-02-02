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

#include "adbfront.h"
#include "extension.h"
#include "network.h"

#if !NDEBUG
#define SHOW_SERVICE_BY_DEBUG 1
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

enum PageIndex
{
    AuthPage = 0,
    LoaderPage,
    CabinetPage,
    LongInfoPage,
    DevicesPage,
    MyDevicesPage,

    LengthPages
};

class Service;
class UnavailableService;
class AdsKillerService;
class StorageCacheCleanService;
class ApkManagerService;
class ContactFixerService;
class MiDeviceUnlockService;

class Service : public QObject
{
    Q_OBJECT

protected:
    DeviceConnectType mDeviceConnectType;
    AdbDevice mAdbDevice;

public:
    QString title;
    QWidget *ownerWidget;
    bool active;

    inline Service(DeviceConnectType deviceConnectType, QObject *parent = nullptr) : QObject(parent), ownerWidget(nullptr), title(), active(false), mDeviceConnectType(deviceConnectType)
    {
    }

    virtual void setArgs(const AdbDevice &adbDevice);

    virtual QString uuid() const = 0;
    virtual bool isAvailable() const;
    virtual PageIndex targetPage();
    virtual bool canStart();
    virtual bool isStarted() = 0;
    virtual bool isFinish() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual QString widgetIconName();

    DeviceConnectType deviceConnectType() const;

    static std::list<std::shared_ptr<Service>> EnumAppServices(QObject *parent = nullptr);
};

class UnavailableService : public Service
{
    Q_OBJECT

public:
    UnavailableService(QObject *parent = nullptr);

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
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

public:
    AdsKillerService(QObject *parent = nullptr);

    void setArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    PageIndex targetPage() override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
    QString widgetIconName() override;
};

class StorageCacheCleanService : public Service
{
    Q_OBJECT

public:
    StorageCacheCleanService(QObject *parent = nullptr);

    void setArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};

class MyDeviceService : public Service
{
    Q_OBJECT
public:
    MyDeviceService(QObject *parent = nullptr);
    ~MyDeviceService();

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
    QString widgetIconName() override;

public slots:
    void slotPullMyDeviceList(const QList<DeviceItemInfo> actual, const QList<DeviceItemInfo> expired, bool ok);
    void slotQuaranteeUpdate();

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

private slots:
    void slotRefresh();
};

class BoostRamService : public Service
{
    Q_OBJECT

public:
    BoostRamService(QObject *parent = nullptr);
    ~BoostRamService();

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};

class ContactFixerService : public Service
{
private:
    struct CFSInternalData *mInternal;

public:
    ContactFixerService(QObject *parent = nullptr);
    ~ContactFixerService();

    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};

class MiDeviceUnlockService : public Service
{
public:
    MiDeviceUnlockService(QObject *parent = nullptr);
    QString uuid() const override;
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};
