#include "Services.h"

BoostRamService::BoostRamService(QObject *parent) : Service(DeviceConnectType::ADB, parent)
{}

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
    
}

bool BoostRamService::isFinish()
{
    
}

bool BoostRamService::start()
{
    
}

void BoostRamService::stop()
{
    
}

void BoostRamService::reset()
{
    
}
