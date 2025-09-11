#include "Services.h"

StorageCacheCleanService::StorageCacheCleanService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{

}

void StorageCacheCleanService::setDevice(const AdbDevice &adbDevice)
{
    Service::setDevice(adbDevice);
}

bool StorageCacheCleanService::canStart()
{
    return Service::canStart();
}

bool StorageCacheCleanService::isStarted()
{
    return false;
}

bool StorageCacheCleanService::isFinish()
{
    return false;
}

bool StorageCacheCleanService::start()
{
    return false;
}

void StorageCacheCleanService::stop()
{
}

void StorageCacheCleanService::reset()
{
}
