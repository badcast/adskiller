#include "Services.h"

MiDeviceUnlockService::MiDeviceUnlockService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
}

QString MiDeviceUnlockService::uuid() const
{
    return IDServiceMiUnlockString;
}

bool MiDeviceUnlockService::canStart()
{
    return Service::canStart();
}

bool MiDeviceUnlockService::isStarted()
{
    return false;
}

bool MiDeviceUnlockService::isFinish()
{
    return false;
}

bool MiDeviceUnlockService::start()
{
    return false;
}

void MiDeviceUnlockService::stop()
{
}

void MiDeviceUnlockService::reset()
{
}
