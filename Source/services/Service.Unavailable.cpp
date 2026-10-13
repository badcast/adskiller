#include "Services.h"

UnavailableService::UnavailableService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
}

QString UnavailableService::uuid() const
{
    return {};
}

QString UnavailableService::getInternalData()
{
    return Service::getInternalData() + ":Availability = unavailable";
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