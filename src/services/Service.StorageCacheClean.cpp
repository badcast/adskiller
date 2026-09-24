#include "Services.h"

QString StorageCacheCleanService::uuid() const
{
    return IDServiceStorageCleanString;
}

StorageCacheCleanService::StorageCacheCleanService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
}

void StorageCacheCleanService::setAndroidArgs(const AdbDevice &adbDevice)
{
    Service::setAndroidArgs(adbDevice);
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
    sendCheckPull();
    return false;
}

void StorageCacheCleanService::stop()
{
}

QString StorageCacheCleanService::widgetIconName()
{
    return "storage-clean";
}
