#include "Services.h"

BoostRamService::BoostRamService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{
}

BoostRamService::~BoostRamService()
{
}

QString BoostRamService::uuid() const
{
    return IDServiceBoostRamString;
}

bool BoostRamService::canStart()
{
    return Service::canStart();
}

bool BoostRamService::isStarted()
{
    return false;
}

bool BoostRamService::isFinish()
{
    return false;
}

bool BoostRamService::start()
{
    return false;
}

void BoostRamService::stop()
{
}

void BoostRamService::reset()
{
}
