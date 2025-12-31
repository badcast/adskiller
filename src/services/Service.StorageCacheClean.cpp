#include "Services.h"

QString StorageCacheCleanService::uuid() const
{
    return IDServiceStorageCleanString;
}

StorageCacheCleanService::StorageCacheCleanService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
}

void StorageCacheCleanService::setArgs(const AdbDevice &adbDevice)
{
    Service::setArgs(adbDevice);
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
