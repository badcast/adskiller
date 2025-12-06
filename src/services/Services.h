#pragma once

#include <memory>

#include <QTimer>
#include <QHash>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QStringListModel>
#include <QCryptographicHash>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QEventLoop>
#include <QLabel>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>
#include <QTableView>
#include <QList>
#include <QLineEdit>
#include <QDateEdit>

#include "extension.h"
#include "network.h"
#include "adbfront.h"

constexpr auto DefaultIconWidget = "unavailable";

constexpr auto IDServiceAdsString          = "0b9d1650-7a10-4fd5-a10e-53fc7f185b1b";
constexpr auto IDServiceMyDeviceString     = "3db562cd-e448-4fc4-aeea-bc13f74ce5c9";
constexpr auto IDServiceAPKManagerString   = "7193decc-f630-4d46-84cf-49059d9f4df5";
constexpr auto IDServiceStorageCleanString = "2ab13aa9-5051-4167-a024-3fbdcde11792";
constexpr auto IDServiceBoostRamString     = "be1f68f6-0f91-4472-947a-07dbe313ab73";
constexpr auto IDServiceWhatsAppMoveString = "95bdb8a2-06f9-4d00-9625-a2da334001e6";

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

class Service : public QObject
{
    Q_OBJECT

protected:
    DeviceConnectType mDeviceConnectType;
    AdbDevice mAdbDevice;

public:
    QString title;
    QWidget * ownerWidget;
    bool active;

    inline Service(DeviceConnectType deviceConnectType, QObject * parent = nullptr) : QObject(parent),
        ownerWidget(nullptr),
        title(),
        active(false),
        mDeviceConnectType(deviceConnectType)
    {}

    virtual void setDevice(const AdbDevice& adbDevice);

    virtual QString uuid() const = 0;
    virtual bool isAvailable() const = 0;
    virtual PageIndex targetPage();
    virtual bool canStart();
    virtual bool isStarted() = 0;
    virtual bool isFinish()= 0;
    virtual bool start()= 0;
    virtual void stop()= 0;
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
    inline bool isAvailable() const override
    {
        return false;
    }
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
    QListView * processLogStatus;
    QLabel * malwareStatusText0;
    QLabel * deviceLabelName;
    QProgressBar * processBarStatus;
    QPushButton * pushButtonReRun;

    void cirlceMalwareState(bool success);
    void cirlceMalwareStateReset();

public:
    AdsKillerService(QObject *parent = nullptr);

    void setDevice(const AdbDevice& adbDevice) override;

    QString uuid() const override;
    inline bool isAvailable() const override
    {
        return true;
    }
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

    void setDevice(const AdbDevice& adbDevice) override;

    QString uuid() const override;
    inline bool isAvailable() const override
    {
        return true;
    }
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

private:
    int mInternalData;
    QTableView * table;
    QDateEdit * dateEditBegin;
    QDateEdit * dateEditEnd;
    QPushButton * refreshButton;
    QCheckBox * quaranteeFilter;
    std::shared_ptr<QList<DeviceItemInfo>> actual;
    std::shared_ptr<QList<DeviceItemInfo>> expired;

    void clearMyDevicesPage(QString text);
    void fillMyDevicesPage();

private slots:
    void slotRefresh();

public slots:
    void slotPullMyDeviceList(const QList<DeviceItemInfo> actual, const QList<DeviceItemInfo> expired, bool ok);
    void slotQuaranteeUpdate();

public:
    MyDeviceService(QObject * parent = nullptr);
    ~MyDeviceService();

    QString uuid() const override;
    inline bool isAvailable() const override
    {
        return true;
    }
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};

class BoostRamService : public Service
{
    Q_OBJECT

public:
    BoostRamService(QObject * parent = nullptr);
    ~BoostRamService();

    QString uuid() const override;
    inline bool isAvailable() const override
    {
        return true;
    }
    bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    bool start() override;
    void stop() override;
    void reset() override;
};
