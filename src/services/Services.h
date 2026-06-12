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

#include "adbfront.h"
#include "extension.h"
#include "network.h"

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

enum PageIndex
{
    AuthPage = 0,
    CabinetPage = 1,
    LongInfoPage = 2,
    BuyVIPPage = 3,
    MyDevicesPage = 4,
    DevicesPage = 5,
    ContactFixerPage = 6,
    FileExplorerPage = 7,
    PAGE_MAX
};

class Service;
class UnavailableService;
class AdsKillerService;
class StorageCacheCleanService;
class BuyVIPService;
class ApkManagerService;
class ContactFixerService;
class MiDeviceUnlockService;
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

    Q_INVOKABLE virtual QString uuid() const = 0;
    Q_INVOKABLE virtual bool isAvailable() const;
    Q_INVOKABLE virtual PageIndex targetPage();
    Q_INVOKABLE virtual bool canStart();
    Q_INVOKABLE virtual bool isStarted() = 0;
    Q_INVOKABLE virtual bool isFinish() = 0;
    Q_INVOKABLE virtual bool start() = 0;
    Q_INVOKABLE virtual void stop() = 0;
    Q_INVOKABLE virtual QString widgetIconName();

    bool restart();
    void close();

    DeviceConnectType deviceConnectType() const;

public:
    static std::list<std::shared_ptr<Service>> EnumAppServices(QObject *parent = nullptr);
};

class UnavailableService : public Service
{
    Q_OBJECT

public:
    UnavailableService(QObject *parent = nullptr);

    QString uuid() const override;
    PageIndex targetPage() override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
};

class AdsKillerService : public Service
{
    Q_OBJECT

    Q_PROPERTY(QStringList logs READ logs NOTIFY logsChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(bool successState READ successState NOTIFY successStateChanged)
    Q_PROPERTY(QString sysOsVersion READ sysOsVersion NOTIFY sysInfoChanged)
    Q_PROPERTY(QString sysStorage READ sysStorage NOTIFY sysInfoChanged)
    Q_PROPERTY(QString sysRam READ sysRam NOTIFY sysInfoChanged)
    Q_PROPERTY(QString sysKernel READ sysKernel NOTIFY sysInfoChanged)
    Q_PROPERTY(QString sysModel READ sysModel NOTIFY sysInfoChanged)
    Q_PROPERTY(QString sysVendor READ sysVendor NOTIFY sysInfoChanged)

private:
    void cirlceMalwareState(bool success);
    void cirlceMalwareStateReset();

    QStringList m_logs;
    int m_progress = 0;
    QString m_statusText;
    QString m_deviceName;
    bool m_isRunning = false;
    bool m_successState = false;
    QString m_sysOsVersion;
    QString m_sysStorage;
    QString m_sysRam;
    QString m_sysKernel;
    QString m_sysModel;
    QString m_sysVendor;

public slots:
    void onPullServiceUUID(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

public:
    struct PrivateKillerRes *_priv;
    AdsKillerService(QObject *parent = nullptr);
    ~AdsKillerService();

    QStringList logs() const
    {
        return m_logs;
    }
    int progress() const
    {
        return m_progress;
    }
    QString statusText() const
    {
        return m_statusText;
    }
    QString deviceName() const
    {
        return m_deviceName;
    }
    bool isRunning() const
    {
        return m_isRunning;
    }
    bool successState() const
    {
        return m_successState;
    }
    QString sysOsVersion() const
    {
        return m_sysOsVersion;
    }
    QString sysStorage() const
    {
        return m_sysStorage;
    }
    QString sysRam() const
    {
        return m_sysRam;
    }
    QString sysKernel() const
    {
        return m_sysKernel;
    }
    QString sysModel() const
    {
        return m_sysModel;
    }
    QString sysVendor() const
    {
        return m_sysVendor;
    }

    void setLogs(const QStringList &l)
    {
        m_logs = l;
        emit logsChanged();
    }
    void setProgress(int p)
    {
        if(m_progress != p)
        {
            m_progress = p;
            emit progressChanged();
        }
    }
    void setStatusText(const QString &s)
    {
        if(m_statusText != s)
        {
            m_statusText = s;
            emit statusTextChanged();
        }
    }
    void setDeviceName(const QString &d)
    {
        if(m_deviceName != d)
        {
            m_deviceName = d;
            emit deviceNameChanged();
        }
    }
    void setIsRunning(bool r)
    {
        if(m_isRunning != r)
        {
            m_isRunning = r;
            emit isRunningChanged();
        }
    }
    void setSuccessState(bool s)
    {
        if(m_successState != s)
        {
            m_successState = s;
            emit successStateChanged();
        }
    }
    void setSysOsVersion(const QString &v)
    {
        if(m_sysOsVersion != v)
        {
            m_sysOsVersion = v;
            emit sysInfoChanged();
        }
    }
    void setSysStorage(const QString &v)
    {
        if(m_sysStorage != v)
        {
            m_sysStorage = v;
            emit sysInfoChanged();
        }
    }
    void setSysRam(const QString &v)
    {
        if(m_sysRam != v)
        {
            m_sysRam = v;
            emit sysInfoChanged();
        }
    }
    void setSysKernel(const QString &v)
    {
        if(m_sysKernel != v)
        {
            m_sysKernel = v;
            emit sysInfoChanged();
        }
    }
    void setSysModel(const QString &v)
    {
        if(m_sysModel != v)
        {
            m_sysModel = v;
            emit sysInfoChanged();
        }
    }
    void setSysVendor(const QString &v)
    {
        if(m_sysVendor != v)
        {
            m_sysVendor = v;
            emit sysInfoChanged();
        }
    }

signals:
    void logsChanged();
    void progressChanged();
    void statusTextChanged();
    void deviceNameChanged();
    void isRunningChanged();
    void successStateChanged();
    void sysInfoChanged();

public:
    void setArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    PageIndex targetPage() override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
    QString widgetIconName() override;
};

class StorageCacheCleanService : public Service
{
    Q_OBJECT

public:
    StorageCacheCleanService(QObject *parent = nullptr);

    void setArgs(const AdbDevice &adbDevice) override;

    QString uuid() const override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
};

class BuyVIPService : public Service
{
    Q_OBJECT

    Q_PROPERTY(QString balanceText READ balanceText NOTIFY balanceTextChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY infoTextChanged)
    Q_PROPERTY(QStringList variants READ variants NOTIFY variantsChanged)

public:
    BuyVIPService(QObject *parent = nullptr);
    ~BuyVIPService();

    QString uuid() const override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    PageIndex targetPage() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
    QString widgetIconName() override;

    QString balanceText() const
    {
        return m_balanceText;
    }
    QString infoText() const
    {
        return m_infoText;
    }
    QStringList variants() const
    {
        return m_variants;
    }

    Q_INVOKABLE void selectVariant(int index);
    Q_INVOKABLE void buyVip(int index);

signals:
    void balanceTextChanged();
    void infoTextChanged();
    void variantsChanged();

private slots:
    void service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

private:
    Network *network;
    std::uint32_t dailyRate;
    int mind, maxd;
    QList<std::tuple<QString, int>> mPresets;

    QString m_balanceText;
    QString m_infoText;
    QStringList m_variants;

    void setBalanceText(const QString &b)
    {
        if(m_balanceText != b)
        {
            m_balanceText = b;
            emit balanceTextChanged();
        }
    }
    void setInfoText(const QString &i)
    {
        if(m_infoText != i)
        {
            m_infoText = i;
            emit infoTextChanged();
        }
    }
    void setVariants(const QStringList &v)
    {
        m_variants = v;
        emit variantsChanged();
    }
};

class MyDeviceService : public Service
{
    Q_OBJECT

    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)

public:
    MyDeviceService(QObject *parent = nullptr);
    ~MyDeviceService();

    QString uuid() const override;
    PageIndex targetPage() override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
    QString widgetIconName() override;

    QVariantList devices() const
    {
        return m_devices;
    }
    bool isRefreshing() const
    {
        return m_isRefreshing;
    }

    Q_INVOKABLE void refreshData(const QString &dateStartISO, const QString &dateEndISO, bool guaranteeFilter);
    Q_INVOKABLE void filterData(bool guaranteeFilter);

signals:
    void devicesChanged();
    void isRefreshingChanged();

public slots:
    void slotPullMyDeviceList(const QJsonObject responce, const QString guid, ServiceOperation so, bool ok);

private:
    int mInternalData;
    std::shared_ptr<QList<DeviceItemInfo>> actual;
    std::shared_ptr<QList<DeviceItemInfo>> expired;

    QVariantList m_devices;
    bool m_isRefreshing = false;
    bool m_lastGuaranteeFilter = true;

    void setIsRefreshing(bool r)
    {
        if(m_isRefreshing != r)
        {
            m_isRefreshing = r;
            emit isRefreshingChanged();
        }
    }
    void buildDeviceList(bool guaranteeFilter);
};

class BoostRamService : public Service
{
    Q_OBJECT

public:
    BoostRamService(QObject *parent = nullptr);
    ~BoostRamService();

    QString uuid() const override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
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
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;

    PageIndex targetPage() override;
    QString widgetIconName() override;

    Q_INVOKABLE QVariantList loadVcf(const QString &path);
    Q_INVOKABLE bool exportVcf(const QVariantList &contacts, const QString &path);
    Q_INVOKABLE QVariantMap parseNumber(const QString &number);
};

class MiDeviceUnlockService : public Service
{
    Q_OBJECT

public:
    MiDeviceUnlockService(QObject *parent = nullptr);
    QString uuid() const override;
    Q_INVOKABLE bool canStart() override;
    bool isStarted() override;
    bool isFinish() override;
    Q_INVOKABLE bool start() override;
    void stop() override;
};

class FileExplorerService : public Service
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY currentPathChanged)
public:
    explicit FileExplorerService(QObject *parent = nullptr);
    ~FileExplorerService();

    bool start() override;
    void stop() override;
    bool isStarted() override
    {
        return mIsRunning;
    }
    bool isFinish() override
    {
        return mSuccessState;
    }

    QString widgetIconName() override
    {
        return "qrc:/service-icons/res/icons/dark-media-transfer.png";
    }
    QString uuid() const override
    {
        return "f0000000-0000-0000-0000-000000000001";
    }
    PageIndex targetPage() override
    {
        return FileExplorerPage;
    }

    QString currentPath() const
    {
        return m_currentPath;
    }
    void setCurrentPath(const QString &p)
    {
        if(m_currentPath != p)
        {
            m_currentPath = p;
            emit currentPathChanged();
        }
    }

    Q_INVOKABLE QVariantList listFiles(const QString &path);
    Q_INVOKABLE bool deleteFile(const QString &path);
    Q_INVOKABLE bool pushFile(const QString &localPath, const QString &remotePath);
    Q_INVOKABLE bool pullFile(const QString &remotePath, const QString &localPath);

signals:
    void currentPathChanged();

private slots:
    void service_uuid_responce(const QJsonObject responce, const QString uuid, ServiceOperation so, bool ok);

private:
    QString m_currentPath = "/sdcard/";
    struct PrivateRes;
    PrivateRes *_priv;
    bool mIsRunning = false;
    bool mSuccessState = false;
};
