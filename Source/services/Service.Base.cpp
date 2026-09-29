#include "Services.h"
#include "mainwindow.h"

void Service::setAndroidArgs(const AdbDevice &adbDevice)
{
    mAdbDevice = adbDevice;
}

void Service::setAppleArgs(const AppleDevice &appleDevice)
{
    mAppleDevice = appleDevice;
}

bool Service::isAvailable() const
{
    return dynamic_cast<const UnavailableService *const>(this) == nullptr;
}

PageIndex Service::targetPage()
{
    return LongInfoPage;
}

bool Service::canStart()
{
    if(mDeviceConnectType == DeviceConnectType::ADB)
        return !mAdbDevice.isEmpty();
    if(mDeviceConnectType == DeviceConnectType::Apple)
        return !mAppleDevice.isEmpty();
    return mDeviceConnectType == DeviceConnectType::None;
}

QString Service::widgetIconName()
{
    return DefaultIconWidget;
}

bool Service::restart()
{
    stop();
    return start();
}

void Service::close()
{
    if(isStarted())
    {
        stop();
        MainWindow::current->updateCabinet();
    }
}

DeviceConnectType Service::deviceConnectType() const
{
    return mDeviceConnectType;
}

bool Service::isOnlineService() const
{
    const QString id = uuid();
    return id == IDServiceAdsString || id == IDServiceMyDeviceString || id == IDServiceVIPBuyString || id == IDServiceAIAgentString || id == IDServiceAITranslaterString;
}

void Service::sendCheckPull() const
{
    if(MainWindow::current && MainWindow::current->network.isAuthed())
    {
        const QString id = uuid();
        if(!id.isEmpty())
        {
            QJsonObject req;
            req[QStringLiteral("check")] = true;
            MainWindow::current->network.pullServiceUUID(id, req, ServiceOperation::Get);
        }
    }
}

std::list<std::shared_ptr<Service>> Service::EnumAppServices(QObject *parent)
{
    std::list<std::shared_ptr<Service>> services;
    services.emplace_back(std::move(std::make_shared<AdsKillerService>(parent)));
    services.emplace_back(std::move(std::make_shared<AppleIpswService>(parent)));
    services.emplace_back(std::move(std::make_shared<MyDeviceService>(parent)));
    services.emplace_back(std::move(std::make_shared<StorageCacheCleanService>(parent)));
    services.emplace_back(std::move(std::make_shared<BoostRamService>(parent)));
    services.emplace_back(std::move(std::make_shared<ContactFixerService>(parent)));
    services.emplace_back(std::move(std::make_shared<MiDeviceUnlockService>(parent)));
    services.emplace_back(std::move(std::make_shared<BuyVIPService>(parent)));
    services.emplace_back(std::move(std::make_shared<AIAgentService>(parent)));
    services.emplace_back(std::move(std::make_shared<FileManagerService>(parent)));
    services.emplace_back(std::move(std::make_shared<ApkManagerService>(parent)));
    services.emplace_back(std::move(std::make_shared<AITranslaterService>(parent)));
    return services;
}

// ------------ UNAVAILABLE SERVICE  ------------

UnavailableService::UnavailableService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
}

QString UnavailableService::uuid() const
{
    return {};
}

PageIndex UnavailableService::targetPage()
{
    return {};
}

bool UnavailableService::canStart()
{
    return false;
}

bool UnavailableService::isStarted()
{
    return false;
}

bool UnavailableService::isFinish()
{
    return false;
}

bool UnavailableService::start()
{
    return false;
}

void UnavailableService::stop()
{
}
