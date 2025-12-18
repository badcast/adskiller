#include "Services.h"

void Service::setDevice(const AdbDevice& adbDevice){
    mAdbDevice = adbDevice;
}

bool Service::isAvailable() const {
    return dynamic_cast<const UnavailableService* const>(this) == nullptr;
}

PageIndex Service::targetPage()
{
    return LongInfoPage;
}

bool Service::canStart()
{
    return mDeviceConnectType == DeviceConnectType::ADB && !mAdbDevice.isEmpty() || mDeviceConnectType == DeviceConnectType::None;
}

QString Service::widgetIconName()
{
    return DefaultIconWidget;
}

DeviceConnectType Service::deviceConnectType() const
{
    return mDeviceConnectType;
}

std::list<std::shared_ptr<Service>> Service::EnumAppServices(QObject * parent)
{
    std::list<std::shared_ptr<Service>> services;
    services.emplace_back(std::move(std::make_shared<AdsKillerService>(parent)));
    services.emplace_back(std::move(std::make_shared<MyDeviceService>(parent)));
    services.emplace_back(std::move(std::make_shared<StorageCacheCleanService>(parent)));
    services.emplace_back(std::move(std::make_shared<BoostRamService>(parent)));
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

void UnavailableService::reset()
{

}
