#include "Services.h"

QString BuyVIPService::uuid() const
{
    return IDServiceVIPBuyString;
}

QString BuyVIPService::widgetIconName()
{
    return "white-transfer";
}

BuyVIPService::BuyVIPService(QObject *parent) : Service(DeviceConnectType::None, parent)
{
}

void BuyVIPService::setArgs(const AdbDevice &adbDevice)
{
    Service::setArgs(adbDevice);
}

bool BuyVIPService::canStart()
{
    return Service::canStart();
}

bool BuyVIPService::isStarted()
{
    return false;
}

PageIndex BuyVIPService::targetPage()
{
    return PageIndex::BuyVIPPage;
}

bool BuyVIPService::isFinish()
{
    return false;
}

bool BuyVIPService::start()
{
    return false;
}

void BuyVIPService::stop()
{
}

void BuyVIPService::reset()
{
    stop();
}
